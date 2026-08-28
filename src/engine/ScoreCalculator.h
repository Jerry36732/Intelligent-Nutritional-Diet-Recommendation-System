#ifndef SCORECALCULATOR_H
#define SCORECALCULATOR_H

#include "../entities/Recipe.h"

class ScoreCalculator
{
public:
    /**
     * Higher is better.
     * Score = -(|calDiff|/targetCal*100) - 2*(|proteinDiff|/targetProtein*100)
     */
    static double evaluate(const Recipe &recipe, double targetCal, double targetProtein);
};

#endif // SCORECALCULATOR_H
