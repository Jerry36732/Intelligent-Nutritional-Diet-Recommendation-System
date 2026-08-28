#include "ScoreCalculator.h"

#include <cmath>

double ScoreCalculator::evaluate(const Recipe &recipe, double targetCal, double targetProtein)
{
    if (targetCal <= 0.0)
        return -1e9;

    const double calPenalty =
        (std::fabs(recipe.totalCalories - targetCal) / targetCal) * 100.0;

    double proteinPenalty = 0.0;
    if (targetProtein > 0.0) {
        proteinPenalty =
            (std::fabs(recipe.totalProtein - targetProtein) / targetProtein) * 100.0;
    }

    return -calPenalty - (2.0 * proteinPenalty);
}
