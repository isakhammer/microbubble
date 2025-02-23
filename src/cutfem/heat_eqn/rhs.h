#ifndef MICROBUBBLE_CUTFEM_POISSON_RHS_H
#define MICROBUBBLE_CUTFEM_POISSON_RHS_H

#include <deal.II/base/function.h>
#include <deal.II/base/point.h>

#include "../utils/scalar_problem.h"


using namespace dealii;


namespace examples::cut::HeatEquation {

    using namespace utils::problems::scalar;

    template<int dim>
    class RightHandSide : public Function<dim> {
    public:
        RightHandSide(const double nu,
                      const double tau,
                      const double center_x = 0,
                      const double center_y = 0);

        double
        value(const Point<dim> &p,
              const unsigned int component = 0) const override;

    private:
        const double nu;
        const double tau;
        const double center_x;
        const double center_y;
    };

    template<int dim>
    class BoundaryValues : public Function<dim> {
    public:
        BoundaryValues(const double center_x = 0,
                       const double center_y = 0);

        double
        value(const Point<dim> &p,
              const unsigned int component = 0) const override;

    private:
        const double center_x;
        const double center_y;
    };


    template<int dim>
    class AnalyticalSolution : public Function<dim> {
    public:
        AnalyticalSolution(const double center_x = 0,
                           const double center_y = 0);

        double
        value(const Point<dim> &p, const unsigned int component) const override;

        Tensor<1, dim>
        gradient(const Point<dim> &p,
                 const unsigned int component) const override;

    private:
        const double center_x;
        const double center_y;
    };


    template<int dim>
    class FlowerDomain : public LevelSet<dim> {
    public :
        FlowerDomain(const double center_x = 0,
                     const double center_y = 0);

        double
        value(const Point<dim> &p, const unsigned int component) const override;

    private:
        const double center_x;
        const double center_y;
    };


    template<int dim>
    class MovingDomain : public LevelSet<dim> {
    public :
        MovingDomain(const double sphere_radius,
                     const double half_length,
                     const double radius);

        double
        value(const Point<dim> &p, const unsigned int component) const override;

        double
        get_speed() override;

        const double sphere_radius;
        const double half_length;
        const double radius;
    };


} // namespace examples::cut::HeatEquation

#endif //MICROBUBBLE_CUTFEM_POISSON_RHS_H
