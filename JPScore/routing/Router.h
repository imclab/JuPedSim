/**
 * \file        Router.h
 * \date        Nov 11, 2010
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
 * virtual base class for all routers.
 * Each router implementation should be derived from this class.
 *
 *
 **/


#ifndef _ROUTER_H
#define  _ROUTER_H

#include <vector>

#include "../general/Macros.h"

class Building;
class Pedestrian;

class Router {

private:
     /// routing strategy ID as defined in the Macros.h file
     RoutingStrategy _strategy;

     /// the id as present in the persons.xml file
     int _id;

protected:

     /// Contain the ids of the intermediate destinations
     std::vector<std::vector<int> >_trips;

     /// All final destinations of the pedestrians
     std::vector<int> _finalDestinations;

public:

     /**
      * Constructor
      */
     Router();
     Router(int id, RoutingStrategy s);

     /**
      * Destructor
      */
     virtual ~Router();

     /**
      * Add a new trip to this router
      * @param trip A vector containing the IDs of the intermediate destination
      */
     void AddTrip(std::vector<int> trip);

     /**
      * Add a new final destination to this router
      * @param id of an intermediate destination as presented in the geometry/routing files
      */
     void AddFinalDestinationID(int id);

     /**
      * @return a vector containing the IDs of the intermediate destinations
      */
     const std::vector<int> GetTrip(int id) const;

     /**
      * @return all final destinations
      */
     const std::vector<int> GetFinalDestinations() const;

     /**
      * Set the id of the router as defined in the person file
      */
     void SetID(int id);

     /**
      * @return the id of the router as defined in the person file
      */
     int GetID() const;

     /**
      * The strategy is automatically set based on the description in the
      * person file.
      */
     void SetStrategy(RoutingStrategy strategy);

     /**
      * The strategy is automatically set based on the description in the
      * person file.
      */
     RoutingStrategy GetStrategy() const;

     /**
      * Find the next suitable target for Pedestrian p
      * @param p the Pedestrian
      * @return -1 in the case no destination could be found
      */
     virtual int FindExit(Pedestrian* p) = 0;

     /**
      * Each implementation of this virtual class has the possibility to initialize
      * its Routing engine using the supplied building object.
      * @param b the building object
      */
     virtual bool Init(Building* b) = 0;

};

#endif  /* _ROUTING_H */

