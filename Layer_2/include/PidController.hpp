#ifndef PID_CONTROLLER_HPP
#define PID_CONTROLLER_HPP

#include <algorithm>

/**
 * ==============================================================================
 *  DISCRETE INDUSTRIAL PID CONTROLLER WITH ANTI-WINDUP
 * ==============================================================================
 * Implements standard ISA velocity/positional PID algorithm:
 *   u(t) = Kp * e(t) + Ki * integral(e) + Kd * de/dt
 * with integral clamping and output saturation.
 * ==============================================================================
 */

class PidController {
public:
    PidController(double kp = 1.0, double ki = 0.0, double kd = 0.0, 
                  double outMin = 0.0, double outMax = 100.0)
        : m_kp(kp), m_ki(ki), m_kd(kd),
          m_outMin(outMin), m_outMax(outMax),
          m_integral(0.0), m_prevError(0.0),
          m_firstRun(true) {}

    void reset() {
        m_integral = 0.0;
        m_prevError = 0.0;
        m_firstRun = true;
    }

    void setTunings(double kp, double ki, double kd) {
        m_kp = kp;
        m_ki = ki;
        m_kd = kd;
    }

    void setOutputLimits(double outMin, double outMax) {
        m_outMin = outMin;
        m_outMax = outMax;
    }

    double calculate(double setpoint, double processVariable, double dt) {
        if (dt <= 0.0) return m_outMin;

        double error = setpoint - processVariable;

        // Proportional term
        double pTerm = m_kp * error;

        // Integral term with anti-windup clamping
        m_integral += error * dt;
        double maxIntegral = (m_ki > 0.0) ? ((m_outMax - m_outMin) / m_ki) : 0.0;
        m_integral = std::clamp(m_integral, -maxIntegral, maxIntegral);
        double iTerm = m_ki * m_integral;

        // Derivative term
        double dTerm = 0.0;
        if (!m_firstRun) {
            double derivative = (error - m_prevError) / dt;
            dTerm = m_kd * derivative;
        } else {
            m_firstRun = false;
        }
        m_prevError = error;

        // Total control output
        double output = pTerm + iTerm + dTerm;
        return std::clamp(output, m_outMin, m_outMax);
    }

private:
    double m_kp;
    double m_ki;
    double m_kd;
    double m_outMin;
    double m_outMax;
    double m_integral;
    double m_prevError;
    bool   m_firstRun;
};

#endif // PID_CONTROLLER_HPP
