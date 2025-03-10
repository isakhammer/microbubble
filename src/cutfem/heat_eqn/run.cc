#include "heat_eqn.h"
#include "rhs.h"


using namespace examples::cut::HeatEquation;


int main(int argc, char *argv[]) {
    Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

    const int dim = 2;
    double radius = 1;
    double half_length = 2 * radius;
    int n_refines = 5;
    int degree = 1;
    bool write_output = true;

    const double nu = 2;
    const double end_time = radius;

    const double time_steps = pow(2, n_refines - 1);
    const double tau = end_time / time_steps;

    RightHandSide<dim> rhs(nu, tau);
    BoundaryValues<dim> bdd;
    AnalyticalSolution<dim> soln;

    double sphere_radius = radius * 0.75;

    // FlowerDomain<dim> domain;
    MovingDomain<dim> domain(sphere_radius, half_length, radius);

    HeatEqn<dim> heat(nu, tau, radius, half_length, n_refines, degree,
                      write_output, rhs, bdd, soln, domain);
    ErrorBase *err = heat.run_time(1, time_steps);
    auto *error = dynamic_cast<ErrorScalar *>(err);
    std::cout << "|| u - u_h ||_L2 = " << error->l2_error << std::endl;
    std::cout << "|| u - u_h ||_H1 = " << error->h1_error << std::endl;
    std::cout << "| u - u_h |_H1 = " << error->h1_semi << std::endl;
}

