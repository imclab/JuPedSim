/**
 * \file        GCFMModel.h
 * \date        Apr 15, 2014
 * \version     v0.6
 * \copyright   <2009-2014> Forschungszentrum Jülich GmbH. All rights reserved.
 *
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * \section Description
 * Implementation of classes for force-based models.
 * Actually we've got two different models:
 * 1. Generalized Centrifugal Force Model
 *
 *
 **/


#ifndef GCFMMODEL_H_
#define GCFMMODEL_H_

#include <vector>

#include "../geometry/Building.h"
#include "OperationalModel.h"



//forward declaration
class Pedestrian;
class DirectionStrategy;


class GCFMModel : public OperationalModel
{
public:

    GCFMModel(DirectionStrategy* dir, double nuped, double nuwall, double dist_effPed, double dist_effWall,
            double intp_widthped, double intp_widthwall, double maxfped, double maxfwall);
    virtual ~GCFMModel(void);

    // Getter
    DirectionStrategy* GetDirection() const;
    double GetNuPed() const;
    double GetNuWall() const;
    double GetDistEffMax() const;
    double GetIntpWidthPed() const;
    double GetIntpWidthWall() const;
    double GetMaxFPed() const;
    double GetMaxFWall() const;
    double GetDistEffMaxPed() const;
    double GetDistEffMaxWall() const;

    /**
     * Compute the next simulation step
     * Solve the differential equations and update the positions and velocities
     * @param current the actual time
     * @param deltaT the next timestep
     * @param building the geometry object
     */
    virtual void ComputeNextTimeStep(double current, double deltaT, Building* building) const;
    virtual std::string GetDescription() const;
    virtual bool Init (Building* building) const;

private:
    /// define the strategy for crossing a door (used for calculating the driving force)
    DirectionStrategy* _direction;
    // Modellparameter
    double _nuPed;                /**< strength of the pedestrian repulsive force */
    double _nuWall;               /**< strength of the wall repulsive force */
    double _intp_widthPed; /**< Interpolation cutoff radius (in cm) */
    double _intp_widthWall; /**< Interpolation cutoff radius (in cm) */
    double _maxfPed;
    double _maxfWall;
    double _distEffMaxPed; // maximal effective distance
    double _distEffMaxWall; // maximal effective distance

    // Private Funktionen
    /**
     * Driving force \f$ F_i =\frac{\mathbf{v_0}-\mathbf{v_i}}{\tau}\f$
     *
     * @param ped Pointer to Pedestrians
     * @param room Pointer to Room
     *
     * @return Point
     */
    Point ForceDriv(Pedestrian* ped, Room* room) const;
    /**
     * Repulsive force between two pedestrians ped1 and ped2 according to
     * the Generalized Centrifugal Force Model (chraibi2010a)
     *
     * @param ped1 Pointer to Pedestrian: First pedestrian
     * @param ped2 Pointer to Pedestrian: Second pedestrian
     *
     * @return Point
     */
    Point ForceRepPed(Pedestrian* ped1, Pedestrian* ped2) const;
    /**
     * Repulsive force acting on pedestrian <ped> from the walls in
     * <subroom>. The sum of all repulsive forces of the walls in <subroom> is calculated
     * @see ForceRepWall
     * @param ped Pointer to Pedestrian
     * @param subroom Pointer to SubRoom
     *
     * @return
     */
    Point ForceRepRoom(Pedestrian* ped, SubRoom* subroom) const;
    Point ForceRepWall(Pedestrian* ped, const Wall& l) const;
    Point ForceRepStatPoint(Pedestrian* ped, const Point& p, double l, double vn) const;
    Point ForceInterpolation(double v0, double K_ij, const Point& e, double v, double d, double r, double l) const;

};


#endif /* GCFMMODEL_H_ */
