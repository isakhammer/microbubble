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

#include <vector>

#include "../utils/flow_problem.h"
#include "rhs.h"


using namespace dealii;
using namespace cutfem;


namespace examples::cut::StokesEquation {

    using NonMatching::LocationToLevelSet;
    using namespace utils::problems::flow;


    template<int dim>
    class StokesEqn : public FlowProblem<dim> {
    public:
        StokesEqn(double nu,
                  double tau,
                  double radius,
                  double half_length,
                  unsigned int n_refines,
                  int element_order,
                  bool write_output,
                  TensorFunction<1, dim> &rhs,
                  TensorFunction<1, dim> &bdd_values,
                  TensorFunction<1, dim> &analytic_vel,
                  Function<dim> &analytic_pressure,
                  LevelSet<dim> &levelset_func,
                  int do_nothing_id = 10,
                  bool stabilized = true,
                  bool stationary = false,
                  bool compute_error = true);

        void
        write_header_to_file(std::ofstream &file);

        void
        write_error_to_file(ErrorBase *error, std::ofstream &file);

    protected:
        void
        set_function_times(double time) override;

        void
        make_grid(Triangulation<dim> &tria) override;

        void
        pre_matrix_assembly() override;

        void
        assemble_matrix() override;

        void
        assemble_matrix_local_over_cell(const FEValues<dim> &fe_values,
                                        const std::vector<types::global_dof_index> &loc2glb) override;

        void
        assemble_matrix_local_over_surface(
                const FEValuesBase<dim> &fe_values,
                const std::vector<types::global_dof_index> &loc2glb) override;

        void
        assemble_rhs(int time_step) override;

        void
        assemble_rhs_local_over_surface(
                const FEValuesBase<dim> &fe_values,
                const std::vector<types::global_dof_index> &loc2glb) override;


        const double nu;

        const double radius;
        const double half_length;

        unsigned int do_nothing_id;

        // Scaling constants for the stabilizations.
        double velocity_stab_scaling = 0;
        double pressure_stab_scaling = 0;
    };

} // namespace examples::cut::StokesEquation


#endif // MICROBUBBLE_STOKES_BDF2_H
