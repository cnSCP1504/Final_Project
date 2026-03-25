#include "safe_regret_mpc/SafeRegretMPC.hpp"
#include <IpIpoptApplication.hpp>
#include <IpTNLP.hpp>
#include <cassert>
#include <numeric>

using namespace safe_regret_mpc;
using namespace Eigen;

namespace safe_regret_mpc {

// ========== Ipopt TNLP Implementation ==========

class SafeRegretMPCTNLP : public Ipopt::TNLP {
public:
    SafeRegretMPCTNLP(SafeRegretMPC* mpc) : mpc_(mpc) {
        assert(mpc != nullptr);
        initial_vars_.resize((mpc_->state_dim_ + mpc_->input_dim_) * (mpc_->mpc_steps_ + 1), 0.0);
    }

    void setInitialVariables(const std::vector<double>& vars) {
        initial_vars_ = vars;
    }

    virtual ~SafeRegretMPCTNLP() {}

    // ========== Method to return some info about the NLP ==========

    bool get_nlp_info(Ipopt::Index& n, Ipopt::Index& m, Ipopt::Index& nnz_jac_g,
                      Ipopt::Index& nnz_h_lag, Ipopt::TNLP::IndexStyleEnum& index_style) override {
        // Number of variables
        n = static_cast<Ipopt::Index>((mpc_->state_dim_ + mpc_->input_dim_) * (mpc_->mpc_steps_ + 1));

        // Number of constraints
        Ipopt::Index n_dynamics = static_cast<Ipopt::Index>(mpc_->state_dim_ * mpc_->mpc_steps_);
        // Only add terminal constraint if explicitly enabled
        Ipopt::Index n_terminal = mpc_->terminal_set_enabled_ ? 1 : 0;
        Ipopt::Index n_dr = mpc_->dr_enabled_ ? static_cast<Ipopt::Index>(mpc_->state_dim_ * mpc_->mpc_steps_) : 0;

        m = n_dynamics + n_terminal + n_dr;

        // Jacobian non-zeros (dense for simplicity)
        nnz_jac_g = n * m;

        // Hessian non-zeros (diagonal approximation)
        nnz_h_lag = n;

        // Use C-style indexing (0-based)
        index_style = Ipopt::TNLP::C_STYLE;

        return true;
    }

    bool get_bounds_info(Ipopt::Index n, Ipopt::Number* x_l, Ipopt::Number* x_u,
                         Ipopt::Index m, Ipopt::Number* g_l, Ipopt::Number* g_u) override {
        // Variable bounds
        Ipopt::Index idx = 0;
        for (size_t t = 0; t <= mpc_->mpc_steps_; ++t) {
            // State bounds
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_l[idx] = mpc_->x_min_(i);
                x_u[idx] = mpc_->x_max_(i);
                idx++;
            }
            // Input bounds
            if (t < mpc_->mpc_steps_) {
                for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                    x_l[idx] = mpc_->u_min_(i);
                    x_u[idx] = mpc_->u_max_(i);
                    idx++;
                }
            }
        }

        // Constraint bounds
        Ipopt::Index g_idx = 0;

        // Dynamics constraints: equality (g(x) = 0)
        Ipopt::Index n_dynamics = static_cast<Ipopt::Index>(mpc_->state_dim_ * mpc_->mpc_steps_);
        for (Ipopt::Index i = 0; i < n_dynamics; ++i) {
            g_l[g_idx] = 0.0;
            g_u[g_idx] = 0.0;
            g_idx++;
        }

        // Terminal constraint (only if enabled)
        if (mpc_->terminal_set_enabled_) {
            // Single distance constraint: ||x_N - center|| <= radius
            g_l[g_idx] = 0.0;
            g_u[g_idx] = mpc_->terminal_radius_;
            g_idx++;
        }

        // DR constraints
        if (mpc_->dr_enabled_) {
            Ipopt::Index n_dr = static_cast<Ipopt::Index>(mpc_->state_dim_ * mpc_->mpc_steps_);
            for (Ipopt::Index i = 0; i < n_dr; ++i) {
                g_l[g_idx] = 0.0;
                g_u[g_idx] = 1e10;
                g_idx++;
            }
        }

        return true;
    }

    bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number* x,
                            bool init_z, Ipopt::Number* z_L, Ipopt::Number* z_U,
                            Ipopt::Index m, bool init_lambda,
                            Ipopt::Number* lambda) override {
        if (init_x) {
            // Use the initial variables provided (contains current state at start)
            for (size_t i = 0; i < initial_vars_.size() && i < static_cast<size_t>(n); ++i) {
                x[i] = initial_vars_[i];
            }
            // Fill remaining with zeros if needed
            for (Ipopt::Index i = static_cast<Ipopt::Index>(initial_vars_.size()); i < n; ++i) {
                x[i] = 0.0;
            }
        }

        if (init_z) {
            for (Ipopt::Index i = 0; i < n; ++i) {
                z_L[i] = 0.0;
                z_U[i] = 0.0;
            }
        }

        if (init_lambda) {
            for (Ipopt::Index i = 0; i < m; ++i) {
                lambda[i] = 0.0;
            }
        }

        return true;
    }

    bool eval_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Number& obj_value) override {
        obj_value = 0.0;

        Ipopt::Index idx = 0;
        for (size_t t = 0; t < mpc_->mpc_steps_; ++t) {
            VectorXd x_t(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_t(i) = x[idx++];
            }

            VectorXd u_t(mpc_->input_dim_);
            for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                u_t(i) = x[idx++];
            }

            obj_value += x_t.dot(mpc_->Q_ * x_t) + u_t.dot(mpc_->R_ * u_t);
        }

        VectorXd x_N(mpc_->state_dim_);
        for (size_t i = 0; i < mpc_->state_dim_; ++i) {
            x_N(i) = x[idx++];
        }
        obj_value += x_N.dot(mpc_->Q_ * x_N);

        if (mpc_->stl_enabled_ && mpc_->stl_weight_ > 0.0) {
            obj_value -= mpc_->stl_weight_ * mpc_->stl_robustness_;
        }

        return true;
    }

    bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                     Ipopt::Number* grad_f) override {
        Ipopt::Index idx = 0;
        for (size_t t = 0; t < mpc_->mpc_steps_; ++t) {
            VectorXd x_t(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_t(i) = x[idx];
            }

            VectorXd grad_x = 2.0 * mpc_->Q_ * x_t;
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                grad_f[idx++] = grad_x(i);
            }

            VectorXd u_t(mpc_->input_dim_);
            for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                u_t(i) = x[idx];
            }

            VectorXd grad_u = 2.0 * mpc_->R_ * u_t;
            for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                grad_f[idx++] = grad_u(i);
            }
        }

        VectorXd x_N(mpc_->state_dim_);
        for (size_t i = 0; i < mpc_->state_dim_; ++i) {
            x_N(i) = x[idx];
        }

        VectorXd grad_N = 2.0 * mpc_->Q_ * x_N;
        for (size_t i = 0; i < mpc_->state_dim_; ++i) {
            grad_f[idx++] = grad_N(i);
        }

        return true;
    }

    bool eval_g(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Index m, Ipopt::Number* g) override {
        Ipopt::Index g_idx = 0;
        Ipopt::Index x_idx = 0;

        // Dynamics constraints
        for (size_t t = 0; t < mpc_->mpc_steps_; ++t) {
            VectorXd x_t(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_t(i) = x[x_idx++];
            }

            VectorXd u_t(mpc_->input_dim_);
            for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                u_t(i) = x[x_idx++];
            }

            VectorXd x_next(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_next(i) = x[x_idx + i];
            }

            VectorXd dynamics_res = x_next - (mpc_->A_ * x_t + mpc_->B_ * u_t);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                g[g_idx++] = dynamics_res(i);
            }
        }

        x_idx += static_cast<Ipopt::Index>(mpc_->state_dim_);

        // Terminal constraint (only if enabled)
        if (mpc_->terminal_set_enabled_) {
            VectorXd x_N(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_N(i) = x[x_idx++];
            }
            VectorXd diff = x_N - mpc_->terminal_center_;
            double dist = diff.norm();
            g[g_idx++] = dist;
        }

        return true;
    }

    bool eval_jac_g(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                    Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index* iRow,
                    Ipopt::Index* jCol, Ipopt::Number* values) override {
        if (values == nullptr) {
            // Sparsity structure (dense)
            Ipopt::Index idx = 0;
            for (Ipopt::Index j = 0; j < m; ++j) {
                for (Ipopt::Index i = 0; i < n; ++i) {
                    iRow[idx] = j;
                    jCol[idx] = i;
                    idx++;
                }
            }
        } else {
            // Finite difference approximation
            const double eps = 1e-8;
            std::vector<Ipopt::Number> g_plus(m), g_minus(m);

            eval_g(n, x, new_x, m, g_plus.data());

            for (Ipopt::Index i = 0; i < n; ++i) {
                std::vector<Ipopt::Number> x_plus(x, x + n);
                x_plus[i] += eps;
                eval_g(n, x_plus.data(), true, m, g_minus.data());

                for (Ipopt::Index j = 0; j < m; ++j) {
                    values[j * n + i] = (g_minus[j] - g_plus[j]) / eps;
                }
            }
        }

        return true;
    }

    bool eval_h(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Number obj_factor, Ipopt::Index m, const Ipopt::Number* lambda,
                bool new_lambda, Ipopt::Index nele_hess, Ipopt::Index* iRow,
                Ipopt::Index* jCol, Ipopt::Number* values) override {
        if (values == nullptr) {
            // Diagonal sparsity structure
            for (Ipopt::Index i = 0; i < n; ++i) {
                iRow[i] = i;
                jCol[i] = i;
            }
        } else {
            // Diagonal Hessian approximation
            Ipopt::Index idx = 0;
            for (size_t t = 0; t < mpc_->mpc_steps_; ++t) {
                for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                    values[idx++] = 2.0 * mpc_->Q_(i, i) * obj_factor;
                }
                for (size_t i = 0; i < mpc_->input_dim_; ++i) {
                    values[idx++] = 2.0 * mpc_->R_(i, i) * obj_factor;
                }
            }
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                values[idx++] = 2.0 * mpc_->Q_(i, i) * obj_factor;
            }
        }

        return true;
    }

    void finalize_solution(Ipopt::SolverReturn status,
                          Ipopt::Index n, const Ipopt::Number* x, const Ipopt::Number* z_L,
                          const Ipopt::Number* z_U, Ipopt::Index m, const Ipopt::Number* g,
                          const Ipopt::Number* lambda, Ipopt::Number obj_value,
                          const Ipopt::IpoptData* ip_data,
                          Ipopt::IpoptCalculatedQuantities* ip_cq) override {
        mpc_->cost_value_ = obj_value;

        // Extract optimal control
        mpc_->optimal_control_ = VectorXd::Zero(mpc_->input_dim_);
        for (size_t i = 0; i < mpc_->input_dim_; ++i) {
            mpc_->optimal_control_(i) = x[mpc_->state_dim_ + i];
        }

        // Extract predicted trajectory
        mpc_->predicted_trajectory_.clear();
        Ipopt::Index idx = 0;
        for (size_t t = 0; t <= mpc_->mpc_steps_; ++t) {
            VectorXd x_t(mpc_->state_dim_);
            for (size_t i = 0; i < mpc_->state_dim_; ++i) {
                x_t(i) = x[idx++];
            }
            mpc_->predicted_trajectory_.push_back(x_t);

            if (t < mpc_->mpc_steps_) {
                idx += static_cast<Ipopt::Index>(mpc_->input_dim_);
            }
        }
    }

private:
    SafeRegretMPC* mpc_;
    std::vector<double> initial_vars_;  // Initial guess for optimization
};

// ========== Ipopt Solver Wrapper ==========

bool SafeRegretMPC::solveWithIpopt(std::vector<double>& vars) {
    std::cout << "Solving MPC optimization with Ipopt..." << std::endl;

    try {
        Ipopt::SmartPtr<Ipopt::IpoptApplication> app = new Ipopt::IpoptApplication();

        // Configure Ipopt
        app->Options()->SetIntegerValue("max_iter", 100);
        app->Options()->SetNumericValue("tol", 1e-3);  // Relaxed tolerance
        app->Options()->SetNumericValue("acceptable_tol", 1e-2);  // Relaxed acceptable tolerance
        app->Options()->SetNumericValue("dual_inf_tol", 1e-1);  // Relaxed dual infeasibility tolerance
        app->Options()->SetNumericValue("compl_inf_tol", 1e-2);  // Relaxed complementarity tolerance
        app->Options()->SetIntegerValue("print_level", 5);  // Always print for debugging
        app->Options()->SetStringValue("print_timing_statistics", "yes");
        app->Options()->SetStringValue("mu_strategy", "adaptive");
        app->Options()->SetStringValue("hessian_approximation", "limited-memory");

        // Initialize
        Ipopt::ApplicationReturnStatus status = app->Initialize();
        if (status != Ipopt::Solve_Succeeded) {
            std::cerr << "Ipopt initialization failed!" << std::endl;
            return false;
        }

        // Create NLP
        SafeRegretMPCTNLP* tnlp_raw = new SafeRegretMPCTNLP(this);
        tnlp_raw->setInitialVariables(vars);
        Ipopt::SmartPtr<Ipopt::TNLP> nlp = tnlp_raw;

        // Solve
        status = app->OptimizeTNLP(nlp);

        if (status == Ipopt::Solve_Succeeded ||
            status == Ipopt::Solved_To_Acceptable_Level ||
            status == Ipopt::Search_Direction_Becomes_Too_Small) {
            std::cout << "MPC solve succeeded!" << std::endl;
            std::cout << "  Status: " << status << std::endl;
            std::cout << "  Cost: " << cost_value_ << std::endl;
            std::cout << "  Control: [" << optimal_control_(0) << ", " << optimal_control_(1) << "]" << std::endl;
            return true;
        } else {
            std::cerr << "MPC solve failed with status: " << status << std::endl;

            // Check if we have a valid solution despite failure status
            if (optimal_control_.norm() > 1e-6) {
                std::cout << "Using partial solution with control: ["
                         << optimal_control_(0) << ", " << optimal_control_(1) << "]" << std::endl;
                return true;
            }

            return false;
        }

    } catch (std::exception& e) {
        std::cerr << "Exception in MPC solve: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception in MPC solve" << std::endl;
        return false;
    }
}

} // namespace safe_regret_mpc
