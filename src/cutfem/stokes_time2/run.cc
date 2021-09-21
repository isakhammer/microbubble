#include "stokes.h"


int main() {
    using namespace TimeDependentStokesBDF2;

    const unsigned int n_refines = 5;
    const unsigned int elementOrder = 1;

    printf("numRefines=%d\n", n_refines);
    printf("elementOrder=%d\n", elementOrder);
    const bool write_vtk = true;

    double radius = 0.205;
    double half_length = 0.41;

    double nu = 1;
    double tau = 0.01;
    unsigned int n_steps = 5;

    double sphere_radius = radius / 4;
    double sphere_x_coord = -half_length / 2;

    const int dim = 2;

    RightHandSide<dim> rhs(nu);
    BoundaryValues<dim> boundary(nu);
    AnalyticalVelocity<dim> analytical_velocity(nu);
    AnalyticalPressure<dim> analytical_pressure(nu);

    StokesCylinder<dim> stokes(radius, half_length, n_refines, nu, tau,
                               elementOrder, write_vtk, rhs, boundary,
                               analytical_velocity, analytical_pressure,
                               sphere_radius, sphere_x_coord);

    Vector<double> u1;
    u1.reinit(1);
    std::vector<Vector<double>> initial = {u1};
    Error error = stokes.run(2, n_steps, initial);

    std::cout << "Mesh size: " << error.mesh_size << std::endl;
    std::cout << "|| u - u_h ||_L2 = " << error.l2_error_u << std::endl;
    std::cout << "|| u - u_h ||_H1 = " << error.h1_error_u << std::endl;
    std::cout << "| u - u_h |_H1 = " << error.h1_semi_u << std::endl;
    std::cout << "|| p - p_h ||_L2 = " << error.l2_error_p << std::endl;
    std::cout << "|| p - p_h ||_H1 = " << error.h1_error_p << std::endl;
    std::cout << "| p - p_h |_H1 = " << error.h1_semi_p << std::endl;
}
