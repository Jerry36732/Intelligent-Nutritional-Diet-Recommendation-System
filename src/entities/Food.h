#ifndef FOOD_H
#define FOOD_H

#include <QString>

struct Food
{
    int id = 0;
    QString name;
    QString category;
    double calories = 0.0;   // kcal / unit
    double protein = 0.0;    // g
    double carbs = 0.0;      // g
    double fat = 0.0;        // g
    QString unit;
};

#endif // FOOD_H
