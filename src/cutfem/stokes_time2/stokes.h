#ifndef MICROBUBBLE_STOKES_BDF2_H
#define MICROBUBBLE_STOKES_BDF2_H

#include <deal.II/base/function.h>
#include <deal.II/base/tensor_function.h>
#include <deal.II/base/point.h>
#include <deal.II/base/quadrature.h>
#include <deal.II/base/tensor.h>

#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_base.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/fe/mapping_q1.h>

#include <deal.II/grid/tria.h>

#include <deal.II/hp/dof_handler.h>
#include <deal.II/hp/fe_collection.h>
#include <deal.II/hp/fe_values.h>
#include <deal.II/hp/mapping_collection.h>
#include <deal.II/hp/q_collection.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/sparsity_pattern.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/solution_transfer.h>

#include <vector>

#include "cutfem/errors/error_calculator.h"

#include "rhs.h"


using namespace dealii;
using namespace cutfem;

namespace TimeDependentStokesBDF2 {

    using NonMatching::LocationToLevelSet;

    template<int dim>
    class StokesCylinder {
    public:
        StokesCylinder(const double radius,
                       const double half_length,
                       const unsigned int n_refines,
                       const double nu,
                       const double tau,
                       const int element_order,
                       const bool write_output,
                       TensorFunction<1, dim> &rhs,
                       TensorFunction<1, dim> &bdd_values,
                       TensorFunction<1, dim> &analytic_vel,
                       Function<dim> &analytic_pressure,
                       const double sphere_radius,
                       TensorFunction<1, dim> &sphere_path_func,
                       const bool crank_nicholson = false);

        /**
         * Run BDF-2.
         *
         * The initial step u_0 is interpolated from the object boundary_values
         * passes as the argument bdd_values in the constructor. If u1 is a
         * vector of length exactly 1, then u1 is also interpolated. Else, the
         * argument u1 is ´used as the start up step u1 for BDF-2.
         *
         * @param u1: is the u1 start up step.
         * @param steps: the number of steps to run.
         * @return an Error object.
         */
        virtual Error
        run(unsigned int bdf_type, unsigned int steps,
            std::vector<Vector<double>> &supplied_solutions);

        virtual Error
        run(unsigned int bdf_type, unsigned int steps);

        Vector<double>
        get_solution();

        static void
        write_header_to_file(std::ofstream &file);

        static void
        write_error_to_file(Error &error, std::ofstream &file);

    protected:
        void
        set_bdf_coefficients(unsigned int bdf_type);

        void
        interpolate_first_steps(unsigned int bdf_type,
                                std::vector<Error> &errors);

        void
        set_supplied_solutions(unsigned int bdf_type,
                               std::vector<Vector<double>> &supplied_solutions,
                               std::vector<Error> &errors);

        void
        interpolate_solutions_onto_updated_grid();

        void
        make_grid();

        void
        setup_level_set();

        void
        setup_quadrature();

        void
        distribute_dofs();

        void
        initialize_matrices();

        void
        assemble_system();

        void
        assemble_local_over_cell(const FEValues<dim> &fe_values,
                                 const std::vector<types::global_dof_index> &loc2glb);

        void
        assemble_local_over_surface(
                const FEValuesBase<dim> &fe_values,
                const std::vector<types::global_dof_index> &loc2glb);

        void
        assemble_rhs();

        void
        assemble_rhs_local_over_cell(
                const FEValues<dim> &fe_v,
                const std::vector<types::global_dof_index> &loc2glb);

        void
        assemble_rhs_local_over_surface(
                const FEValuesBase<dim> &fe_v,
                const std::vector<types::global_dof_index> &loc2glb);


        void
        solve();

        void
        output_results(std::string &suffix,
                       bool output_levelset = true) const;

        void
        output_results(int time_step,
                       bool output_levelset = true) const;

        Error compute_error();

        void integrate_cell(const FEValues<dim> &fe_v,
                            double &l2_error_integral_u,
                            double &h1_error_integral_u,
                            double &l2_error_integral_p,
                            double &h1_error_integral_p,
                            const double &mean_numerical_pressure,
                            const double &mean_exact_pressure) const;

        Error compute_time_error(std::vector<Error> errors);

        void
        write_time_header_to_file(std::ofstream &file);

        void
        write_time_error_to_file(Error &error, std::ofstream &file);

        const double radius;
        const double half_length;
        const unsigned int n_refines;

        const double nu;
        const double tau;

        bool write_output;

        double sphere_radius;
        double sphere_x_coord;

        TensorFunction<1, dim> *rhs_function;
        TensorFunction<1, dim> *boundary_values;
        TensorFunction<1, dim> *sphere_path;
        TensorFunction<1, dim> *analytical_velocity;
        Function<dim> *analytical_pressure;

        // Cell side-length.
        double h;
        const unsigned int element_order;

        unsigned int do_nothing_id = 2;

        Triangulation<dim> triangulation;
        FESystem<dim> stokes_fe;

        hp::FECollection<dim> fe_collection;
        hp::MappingCollection<dim> mapping_collection;
        hp::QCollection<dim> q_collection;
        hp::QCollection<1> q_collection1D;

        // Object managing degrees of freedom for the level set function.
        FE_Q<dim> fe_levelset;
        DoFHandler<dim> levelset_dof_handler;
        Vector<double> levelset;

        // Object managing degrees of freedom for the cutfem method.
        hp::DoFHandler<dim> dof_handler;

        NonMatching::CutMeshClassifier<dim> cut_mesh_classifier;
        SolutionTransfer<dim, Vector<double>> solution_transfer;

        SparsityPattern sparsity_pattern;
        SparseMatrix<double> stiffness_matrix;

        Vector<double> rhs;         // f
        Vector<double> solution;    // u = u^(n+1)
        std::vector<Vector<double>> solutions; // (u0, u1) when BDF-2 is used.

        // Constants used for the time discretization, defined as:
        //   u_t = (au^(n+1) + bu^n + cu^(n-1))/τ, where u = u^(n+1)
        // For BDF-1: (b, a), and (c, b, a) for BDF-2.
        std::vector<double> bdf_coeffs;
        const bool crank_nicholson; // TODO implement

        AffineConstraints<double> constraints;
    };

} // namespace TimeDependentStokesBDF2


#endif // MICROBUBBLE_STOKES_BDF2_H
