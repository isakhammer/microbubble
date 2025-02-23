#include <deal.II/base/data_out_base.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/std_cxx17/optional.h>

#include <deal.II/fe/fe_nothing.h>
#include <deal.II/fe/fe_update_flags.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/tria_accessor.h>

#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/solver_control.h>
#include <deal.II/lac/sparse_direct.h>

#include <deal.II/non_matching/fe_values.h>
#include <deal.II/non_matching/fe_immersed_values.h>

#include <deal.II/numerics/data_out_dof_data.h>
#include <deal.II/numerics/vector_tools.h>

#include <cmath>
#include <fstream>
#include <stdexcept>

#include "../utils/stabilization/jump_stabilization.h"

#include "../utils/utils.h"

#include "stokes.h"


using namespace cutfem;

namespace examples::cut::StokesEquation {

    using namespace utils;

    template<int dim>
    StokesEqn<dim>::
    StokesEqn(const double nu,
              const double tau,
              const double radius,
              const double half_length,
              const unsigned int n_refines,
              const int element_order,
              const bool write_output,
              TensorFunction<1, dim> &rhs,
              TensorFunction<1, dim> &bdd_values,
              TensorFunction<1, dim> &analytic_vel,
              Function<dim> &analytic_pressure,
              LevelSet<dim> &levelset_func,
              const int do_nothing_id,
              const bool stabilized,
              const bool stationary,
              const bool compute_error)
            : FlowProblem<dim>(n_refines, element_order, write_output,
                               levelset_func, analytic_vel, analytic_pressure,
                               stabilized, stationary, compute_error),
              nu(nu), radius(radius), half_length(half_length),
              do_nothing_id(do_nothing_id) {
        this->tau = tau;

        // Use Dirichlet boundary conditions everywhere, this is done by
        // default by constructor definition.
        // this->do_nothing_id = 10;

        this->rhs_function = &rhs;
        this->boundary_values = &bdd_values;
    }

    template<int dim>
    void StokesEqn<dim>::
    set_function_times(double time) {
        this->rhs_function->set_time(time);
        this->boundary_values->set_time(time);
        this->analytical_velocity->set_time(time);
        this->analytical_pressure->set_time(time);

        if (this->moving_domain) {
            this->levelset_function->set_time(time);
        } else {
            this->levelset_function->set_time(0);
        }
    }


    template<int dim>
    void StokesEqn<dim>::
    make_grid(Triangulation<dim> &tria) {
        this->pcout << "Creating triangulation" << std::endl;

        GridGenerator::cylinder(tria, radius, half_length);
        GridTools::remove_anisotropy(tria, 1.618, 5);
        tria.refine_global(this->n_refines);

        this->mapping_collection.push_back(MappingCartesian<dim>());
    }


    template<int dim>
    void StokesEqn<dim>::
    pre_matrix_assembly() {
        this->pcout << "Stabilization constants set for StokesEqn." << std::endl;
        // Set the velocity and pressure stabilization scalings. These can
        // be overridden in a subclass constructor.
        double gamma_u = 0.5;
        double gamma_p = 0.5;
        this->velocity_stab_scaling =
                gamma_u * (1 + this->tau * nu / pow(this->h, 2));
        this->pressure_stab_scaling =
                -gamma_p * this->tau / (nu + pow(this->h, 2) / this->tau);
    }


    template<int dim>
    void StokesEqn<dim>::
    assemble_matrix() {
        this->pcout << "Assembling: Stokes" << std::endl;

        // Object deciding what faces should be stabilized.
        std::shared_ptr<Selector<dim>> face_selector(
                new Selector<dim>(this->cut_mesh_classifier));

        // Use a helper object to compute the stabilisation for both the velocity
        // and the pressure component.
        const FEValuesExtractors::Vector velocities(0);
        stabilization::JumpStabilization<dim, FEValuesExtractors::Vector>
                velocity_stab(*this->dof_handlers.front(),
                              this->mapping_collection,
                              this->cut_mesh_classifier,
                              this->constraints);
        velocity_stab.set_faces_to_stabilize(face_selector);
        velocity_stab.set_weight_function(stabilization::taylor_weights);
        velocity_stab.set_extractor(velocities);

        const FEValuesExtractors::Scalar pressure(dim);
        stabilization::JumpStabilization<dim, FEValuesExtractors::Scalar>
                pressure_stab(*this->dof_handlers.front(),
                              this->mapping_collection,
                              this->cut_mesh_classifier,
                              this->constraints);
        pressure_stab.set_faces_to_stabilize(face_selector);
        pressure_stab.set_weight_function(stabilization::taylor_weights);
        pressure_stab.set_extractor(pressure);

        assert(velocity_stab_scaling != 0);
        assert(pressure_stab_scaling != 0);

        NonMatching::RegionUpdateFlags region_update_flags;
        region_update_flags.inside = update_values | update_JxW_values |
                                     update_gradients |
                                     update_quadrature_points;
        region_update_flags.surface = update_values | update_JxW_values |
                                      update_gradients |
                                      update_quadrature_points |
                                      update_normal_vectors;

        NonMatching::FEValues<dim> cut_fe_values(this->mapping_collection,
                                                 this->fe_collection,
                                                 this->q_collection,
                                                 this->q_collection1D,
                                                 region_update_flags,
                                                 this->cut_mesh_classifier,
                                                 this->levelset_dof_handler,
                                                 this->levelset);

        // Quadrature for the faces of the cells on the outer boundary
        QGauss<dim - 1> face_quadrature_formula(this->mixed_fe.degree + 1);
        FEFaceValues<dim> fe_face_values(this->mixed_fe,
                                         face_quadrature_formula,
                                         update_values | update_gradients |
                                         update_quadrature_points |
                                         update_normal_vectors |
                                         update_JxW_values);


        for (const auto &cell : this->dof_handlers.front()->active_cell_iterators()) {
            if (cell->is_locally_owned()) {
                const unsigned int n_dofs = cell->get_fe().dofs_per_cell;
                const LocationToLevelSet location =
                        this->cut_mesh_classifier.location_to_level_set(cell);
                std::vector<types::global_dof_index> loc2glb(n_dofs);
                cell->get_dof_indices(loc2glb);

                // This call will compute quadrature rules relevant for this cell
                // in the background.
                cut_fe_values.reinit(cell);

                if (location != LocationToLevelSet::outside) {
                    // Retrieve an FEValues object with quadrature points
                    // over the full cell.
                    const std_cxx17::optional<FEValues<dim>>& fe_values_bulk =
                            cut_fe_values.get_inside_fe_values();

                    if (fe_values_bulk) {
                        this->assemble_matrix_local_over_cell(*fe_values_bulk,
                                                            loc2glb);
                    }

                    // Loop through all faces that constitutes the outer boundary of the
                    // domain.
                    for (const auto &face : cell->face_iterators()) {
                        if (face->at_boundary() &&
                            face->boundary_id() != do_nothing_id) {
                            fe_face_values.reinit(cell, face);
                            this->assemble_matrix_local_over_surface(fe_face_values,
                                                                    loc2glb);
                        }
                    }

                    // Retrieve an FEValues object with quadrature points
                    // on the immersed surface.
                    const std_cxx17::optional<FEImmersedSurfaceValues<dim>>&
                            fe_values_surface = cut_fe_values.get_surface_fe_values();

                    if (fe_values_surface)
                        this->assemble_matrix_local_over_surface(*fe_values_surface,
                                                                loc2glb);
                }

                if (this->stabilized) {
                    // Compute and add the velocity stabilization.
                    velocity_stab.compute_stabilization(cell);
                    velocity_stab.add_stabilization_to_matrix(
                            this->velocity_stab_scaling, this->stiffness_matrix);
                    // Compute and add the pressure stabilisation.
                    pressure_stab.compute_stabilization(cell);
                    pressure_stab.add_stabilization_to_matrix(
                            this->pressure_stab_scaling, this->stiffness_matrix);
                }
            }
        }
        this->stiffness_matrix.compress(VectorOperation::add);
    }


    template<int dim>
    void StokesEqn<dim>::
    assemble_matrix_local_over_cell(
            const FEValues<dim> &fe_values,
            const std::vector<types::global_dof_index> &loc2glb) {

        // Matrix and vector for the contribution of each cell
        const unsigned int dofs_per_cell = fe_values.get_fe().dofs_per_cell;
        FullMatrix<double> local_matrix(dofs_per_cell, dofs_per_cell);

        const FEValuesExtractors::Vector velocities(0);
        const FEValuesExtractors::Scalar pressure(dim);

        // Calculate often used terms in the beginning of each cell-loop
        std::vector<Tensor<2, dim>> grad_phi_u(dofs_per_cell);
        std::vector<double> div_phi_u(dofs_per_cell);
        std::vector<Tensor<1, dim>> phi_u(dofs_per_cell, Tensor<1, dim>());
        std::vector<double> phi_p(dofs_per_cell);

        const int time_switch = this->stationary ? 0 : 1;

        for (unsigned int q = 0; q < fe_values.n_quadrature_points; ++q) {
            for (const unsigned int k : fe_values.dof_indices()) {
                grad_phi_u[k] = fe_values[velocities].gradient(k, q);
                div_phi_u[k] = fe_values[velocities].divergence(k, q);
                phi_u[k] = fe_values[velocities].value(k, q);
                phi_p[k] = fe_values[pressure].value(k, q);
            }

            for (const unsigned int i : fe_values.dof_indices()) {
                for (const unsigned int j : fe_values.dof_indices()) {
                    local_matrix(i, j) +=
                            (this->bdf_coeffs[0]
                             * phi_u[j] * phi_u[i] * time_switch // (u, v)
                             +
                             (nu * scalar_product(grad_phi_u[j],
                                                  grad_phi_u[i]) // (grad u, grad v)
                              - (div_phi_u[i] * phi_p[j])   // -(div v, p)
                              - (div_phi_u[j] * phi_p[i])   // -(div u, q)
                             ) * this->tau) *
                            fe_values.JxW(q); // dx
                }
                // NB: rhs is assembled in assemble_rhs().
            }
        }
        this->stiffness_matrix.add(loc2glb, local_matrix);
    }


    template<int dim>
    void StokesEqn<dim>::
    assemble_matrix_local_over_surface(
            const FEValuesBase<dim> &fe_values,
            const std::vector<types::global_dof_index> &loc2glb) {
        // Matrix and vector for the contribution of each cell
        const unsigned int dofs_per_cell = fe_values.get_fe().dofs_per_cell;
        FullMatrix<double> local_matrix(dofs_per_cell, dofs_per_cell);

        const FEValuesExtractors::Vector velocities(0);
        const FEValuesExtractors::Scalar pressure(dim);

        // Calculate often used terms in the beginning of each cell-loop
        std::vector<Tensor<2, dim>> grad_phi_u(dofs_per_cell);
        std::vector<double> div_phi_u(dofs_per_cell);
        std::vector<Tensor<1, dim>> phi_u(dofs_per_cell, Tensor<1, dim>());
        std::vector<double> phi_p(dofs_per_cell);

        // Nitsche penalty parameter
        double mu = 5 * nu * this->element_order * (this->element_order + 1) /
                    this->h;
        Tensor<1, dim> normal;

        for (unsigned int q : fe_values.quadrature_point_indices()) {
            normal = fe_values.normal_vector(q);

            for (const unsigned int k : fe_values.dof_indices()) {
                grad_phi_u[k] = fe_values[velocities].gradient(k, q);
                div_phi_u[k] = fe_values[velocities].divergence(k, q);
                phi_u[k] = fe_values[velocities].value(k, q);
                phi_p[k] = fe_values[pressure].value(k, q);
            }

            for (const unsigned int i : fe_values.dof_indices()) {
                for (const unsigned int j : fe_values.dof_indices()) {
                    local_matrix(i, j) +=
                            (-nu * (grad_phi_u[j] * normal) *
                             phi_u[i]  // -(grad u * n, v)
                             -
                             nu * (grad_phi_u[i] * normal) *
                             phi_u[j] // -(grad v * n, u) [Nitsche]
                             + mu * (phi_u[j] * phi_u[i]) // mu (u, v) [Nitsche]
                             + (normal * phi_u[i]) *
                               phi_p[j]                  // (n * v, p) [from ∇p]
                             + (normal * phi_u[j]) *
                               phi_p[i]                  // (q*n, u) [Nitsche]
                            ) * this->tau *
                            // Multiply all terms with the time step
                            fe_values.JxW(q); // ds
                }
                // NB: rhs is assembled in assemble_rhs().
            }
        }
        this->stiffness_matrix.add(loc2glb, local_matrix);
    }


    template<int dim>
    void StokesEqn<dim>::
    assemble_rhs(int time_step) {
        (void) time_step;
        this->pcout << "Assembling rhs: Stokes" << std::endl;

        NonMatching::RegionUpdateFlags region_update_flags;
        region_update_flags.inside = update_values | update_quadrature_points
                                     |
                                     update_JxW_values; //  | update_gradients;
        region_update_flags.surface =
                update_values | update_JxW_values |
                update_gradients |
                update_quadrature_points |
                update_normal_vectors;

        NonMatching::FEValues<dim> cut_fe_values(this->mapping_collection,
                                                 this->fe_collection,
                                                 this->q_collection,
                                                 this->q_collection1D,
                                                 region_update_flags,
                                                 this->cut_mesh_classifier,
                                                 this->levelset_dof_handler,
                                                 this->levelset);

        // Quadrature for the faces of the cells on the outer boundary
        QGauss<dim - 1> face_quadrature_formula(this->mixed_fe.degree + 1);
        FEFaceValues<dim> fe_face_values(this->mixed_fe,
                                         face_quadrature_formula,
                                         update_values | update_gradients |
                                         update_quadrature_points |
                                         update_normal_vectors |
                                         update_JxW_values);

        // TODO setter dette alle elementene i rhs til 0?
        // rhs = 0;

        for (const auto &cell : this->dof_handlers.front()->active_cell_iterators()) {
            if (cell->is_locally_owned()) {
                const unsigned int n_dofs = cell->get_fe().dofs_per_cell;
                std::vector<types::global_dof_index> loc2glb(n_dofs);
                cell->get_dof_indices(loc2glb);

                // This call will compute quadrature rules relevant for this cell
                // in the background.
                cut_fe_values.reinit(cell);

                // Retrieve an FEValues object with quadrature points
                // over the full cell.
                const std_cxx17::optional<FEValues<dim>>& fe_values_bulk =
                        cut_fe_values.get_inside_fe_values();

                if (fe_values_bulk) {
                    if (this->moving_domain) {
                        this->assemble_rhs_and_bdf_terms_local_over_cell_moving_domain(
                                *fe_values_bulk, loc2glb);
                    } else {
                        this->assemble_rhs_and_bdf_terms_local_over_cell(
                                *fe_values_bulk, loc2glb);
                    }
                }

                // Loop through all faces that constitutes the outer boundary of the
                // domain.
                for (const auto &face : cell->face_iterators()) {
                    if (face->at_boundary() &&
                        face->boundary_id() != do_nothing_id) {
                        fe_face_values.reinit(cell, face);
                        assemble_rhs_local_over_surface(fe_face_values, loc2glb);
                    }
                }

                // Retrieve an FEValues object with quadrature points
                // on the immersed surface.
                const std_cxx17::optional<FEImmersedSurfaceValues<dim>>&
                        fe_values_surface = cut_fe_values.get_surface_fe_values();

                if (fe_values_surface) {
                    assemble_rhs_local_over_surface(*fe_values_surface, loc2glb);
                }
            }
        }
        this->rhs.compress(VectorOperation::add);
    }


    template<int dim>
    void StokesEqn<dim>::
    assemble_rhs_local_over_surface(
            const FEValuesBase<dim> &fe_values,
            const std::vector<types::global_dof_index> &loc2glb) {
        // Matrix and vector for the contribution of each cell
        const unsigned int dofs_per_cell = fe_values.get_fe().dofs_per_cell;
        // FullMatrix<double> local_matrix(dofs_per_cell, dofs_per_cell);
        Vector<double> local_rhs(dofs_per_cell);

        // Evaluate the boundary function for all quadrature points on this face.
        std::vector<Tensor<1, dim>> bdd_values(fe_values.n_quadrature_points,
                                               Tensor<1, dim>());
        this->boundary_values->value_list(fe_values.get_quadrature_points(),
                                          bdd_values);

        const FEValuesExtractors::Vector v(0);
        const FEValuesExtractors::Scalar p(dim);

        // Nitsche penalty parameter
        double mu = 5 * nu * this->element_order * (this->element_order + 1) /
                    this->h;
        Tensor<1, dim> normal;

        for (unsigned int q : fe_values.quadrature_point_indices()) {
            normal = fe_values.normal_vector(q);

            for (const unsigned int i : fe_values.dof_indices()) {
                // These terms comes from Nitsches method.
                Tensor<1, dim> prod_r =
                        mu * fe_values[v].value(i, q) -             // mu v
                        nu * fe_values[v].gradient(i, q) * normal + // nu ∇v n
                        fe_values[p].value(i, q) * normal;          // q * n

                local_rhs(i) +=
                        this->tau * prod_r *
                        bdd_values[q] // (g, mu v - nu ∇v n + q * n)
                        * fe_values.JxW(q);    // ds
            }
        }
        this->rhs.add(loc2glb, local_rhs);
    }


    template<int dim>
    void StokesEqn<dim>::
    write_header_to_file(std::ofstream &file) {
        if (this->this_mpi_process == 0) {
            file << "h, \\tau, \\|u\\|_{L^2L^2}, \\|u\\|_{L^2H^1}, |u|_{L^2H^1}, "
                    "\\|p\\|_{L^2L^2}, \\|p\\|_{L^2H^1}, |p|_{L^2H^1}, "
                    "\\|u\\|_{l^\\infty L^2}, \\|u\\|_{l^\\infty H^1},"
                << std::endl;
        }
    }


    template<int dim>
    void StokesEqn<dim>::
    write_error_to_file(ErrorBase *error, std::ofstream &file) {
        if (this->this_mpi_process == 0) {
            auto *err = dynamic_cast<ErrorFlow *>(error);
            file << err->h << ","
                << err->tau << ","
                << err->l2_error_u << ","
                << err->h1_error_u << ","
                << err->h1_semi_u << ","
                << err->l2_error_p << ","
                << err->h1_error_p << ","
                << err->h1_semi_p << ","
                << err->l_inf_l2_error_u << ","
                << err->l_inf_h1_error_u << std::endl;
        }
    }


    template
    class StokesEqn<2>;

    template
    class StokesEqn<3>;

} // namespace examples::cut::StokesEquation
