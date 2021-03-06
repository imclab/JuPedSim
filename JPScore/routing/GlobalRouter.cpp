/**
 * \file        GlobalRouter.cpp
 * \date        Dec 15, 2010
 * \version     v0.6
 * \copyright   <2009-2014> Forschungszentrum J�lich GmbH. All rights reserved.
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
 *
 *
 **/


#include "GlobalRouter.h"

#include "AccessPoint.h"
#include "Router.h"
#include "NavMesh.h"
#include "DTriangulation.h"

#include "../geometry/Building.h"
#include "../pedestrian/Pedestrian.h"
#include "../tinyxml/tinyxml.h"
#include "../geometry/SubRoom.h"
#include "../geometry/Wall.h"
#include "../IO/OutputHandler.h"

#include <sstream>
#include <cfloat>
#include <fstream>
#include <iomanip>


using namespace std;

GlobalRouter::GlobalRouter() :
Router()
{
     _accessPoints = map<int, AccessPoint*>();
     _map_id_to_index = std::map<int, int>();
     _map_index_to_id = std::map<int, int>();
     _distMatrix = NULL;
     _pathsMatrix = NULL;
     _building = NULL;
     _edgeCost=1;
     //     _rdDistribution = uniform_real_distribution<double> (0,1);
     //     _rdGenerator = default_random_engine(56);

}

GlobalRouter::GlobalRouter(int id, RoutingStrategy s) :  Router(id, s)
{
     _accessPoints = map<int, AccessPoint*>();
     _map_id_to_index = std::map<int, int>();
     _map_index_to_id = std::map<int, int>();
     _distMatrix = NULL;
     _pathsMatrix = NULL;
     _building = NULL;
     _edgeCost=1;

     //     _rdDistribution = uniform_real_distribution<double> (0,1);
     //     _rdGenerator = default_random_engine(56);

}

GlobalRouter::~GlobalRouter()
{
     if (_distMatrix && _pathsMatrix) {
          const int exitsCnt = _building->GetNumberOfGoals();
          for (int p = 0; p < exitsCnt; ++p) {
               delete[] _distMatrix[p];
               delete[] _pathsMatrix[p];
          }

          delete[] _distMatrix;
          delete[] _pathsMatrix;
     }

     map<int, AccessPoint*>::const_iterator itr;
     for (itr = _accessPoints.begin(); itr != _accessPoints.end(); ++itr) {
          delete itr->second;
     }
     _accessPoints.clear();
}

bool GlobalRouter::Init(Building* building)
{
     //necessary if the init is called several times during the simulation
     Reset();
     Log->Write("INFO:\tInit the Global Router Engine");
     _building = building;
     //only load the information if not previously loaded
     //if(_building->GetNumberOfGoals()==0)
     LoadRoutingInfos(GetRoutingInfoFile());

     if(_generateNavigationMesh)
     {
          //GenerateNavigationMesh();
          TriangulateGeometry();
          //return true;
     }

     // initialize the distances matrix for the floydwahrshall
     int exitsCnt = _building->GetNumberOfGoals() + _building->GetAllGoals().size();

     _distMatrix = new double*[exitsCnt];
     _pathsMatrix = new int*[exitsCnt];

     for (int i = 0; i < exitsCnt; ++i) {
          _distMatrix[i] = new double[exitsCnt];
          _pathsMatrix[i] = new int[exitsCnt];
     }

     // Initializing the values
     // all nodes are disconnected
     for (int p = 0; p < exitsCnt; ++p) {
          for (int r = 0; r < exitsCnt; ++r) {
               _distMatrix[p][r] = (r == p) ? 0.0 : FLT_MAX;/*0.0*/
               _pathsMatrix[p][r] = p;/*0.0*/
          }
     }

     // init the access points
     int index = 0;
     for(const auto & itr:_building->GetAllHlines())
     {
          //int door=itr->first;
          int door = itr.second->GetUniqueID();
          Hline* cross = itr.second;
          Point centre = cross->GetCentre();
          double center[2] = { centre.GetX(), centre.GetY() };

          AccessPoint* ap = new AccessPoint(door, center);
          ap->SetNavLine(cross);
          char friendlyName[CLENGTH];
          sprintf(friendlyName, "hline_%d_room_%d_subroom_%d", cross->GetID(),
                    cross->GetRoom1()->GetID(),
                    cross->GetSubRoom1()->GetSubRoomID());
          ap->SetFriendlyName(friendlyName);

          // save the connecting sub/rooms IDs
          int id1 = -1;
          if (cross->GetSubRoom1()) {
               id1 = cross->GetSubRoom1()->GetUID();
          }

          ap->setConnectingRooms(id1, id1);
          _accessPoints[door] = ap;

          //very nasty
          _map_id_to_index[door] = index;
          _map_index_to_id[index] = door;
          index++;
     }

     for(const auto & itr:_building->GetAllCrossings())
     {
          int door = itr.second->GetUniqueID();
          Crossing* cross = itr.second;
          const Point& centre = cross->GetCentre();
          double center[2] = { centre.GetX(), centre.GetY() };

          AccessPoint* ap = new AccessPoint(door, center);
          ap->SetNavLine(cross);
          char friendlyName[CLENGTH];
          sprintf(friendlyName, "cross_%d_room_%d_subroom_%d", cross->GetID(),
                    cross->GetRoom1()->GetID(),
                    cross->GetSubRoom1()->GetSubRoomID());
          ap->SetFriendlyName(friendlyName);

          // save the connecting sub/rooms IDs
          int id1 = -1;
          if (cross->GetSubRoom1()) {
               id1 = cross->GetSubRoom1()->GetUID();
          }

          int id2 = -1;
          if (cross->GetSubRoom2()) {
               id2 = cross->GetSubRoom2()->GetUID();
          }

          ap->setConnectingRooms(id1, id2);
          _accessPoints[door] = ap;

          //very nasty
          _map_id_to_index[door] = index;
          _map_index_to_id[index] = door;
          index++;
     }


     for(const auto & itr:_building->GetAllTransitions())
     {
          int door = itr.second->GetUniqueID();
          Transition* cross = itr.second;
          const Point& centre = cross->GetCentre();
          double center[2] = { centre.GetX(), centre.GetY() };

          AccessPoint* ap = new AccessPoint(door, center);
          ap->SetNavLine(cross);
          char friendlyName[CLENGTH];
          sprintf(friendlyName, "trans_%d_room_%d_subroom_%d", cross->GetID(),
                    cross->GetRoom1()->GetID(),
                    cross->GetSubRoom1()->GetSubRoomID());
          ap->SetFriendlyName(friendlyName);

          ap->SetClosed(!cross->IsOpen());
          // save the connecting sub/rooms IDs
          int id1 = -1;
          if (cross->GetSubRoom1()) {
               id1 = cross->GetSubRoom1()->GetUID();
          }

          int id2 = -1;
          if (cross->GetSubRoom2()) {
               id2 = cross->GetSubRoom2()->GetUID();
          }

          ap->setConnectingRooms(id1, id2);
          _accessPoints[door] = ap;

          //set the final destination
          if (cross->IsExit() && cross->IsOpen()) {
               ap->SetFinalExitToOutside(true);
               Log->Write("INFO: \tExit to outside found: %d [%s]",ap->GetID(),ap->GetFriendlyName().c_str());
          } else if ((id1 == -1) && (id2 == -1)) {
               Log->Write("INFO:\t a final destination outside the geometry was found");
               ap->SetFinalExitToOutside(true);
          } else if (cross->GetRoom1()->GetCaption() == "outside") {
               ap->SetFinalExitToOutside(true);
          }

          //very nasty
          _map_id_to_index[door] = index;
          _map_index_to_id[index] = door;
          index++;
     }

     // loop over the rooms
     // loop over the subrooms
     // get the transitions in the subrooms
     // and compute the distances

     for(auto && itroom:_building->GetAllRooms())
     {
          auto&& room=itroom.second;
          for(const auto & it_sub:room->GetAllSubRooms())
          {
               // The penalty factor should discourage pedestrians to evacuation through rooms
               auto&& sub=it_sub.second;
               double  penalty=1.0;
               if((sub->GetType()!="floor") && (sub->GetType()!="dA") ) {
                    penalty=_edgeCost;
               }

               //collect all navigation objects
               vector<NavLine*> allGoals;
               const auto & crossings = sub->GetAllCrossings();
               allGoals.insert(allGoals.end(), crossings.begin(), crossings.end());
               const auto & transitions = sub->GetAllTransitions();
               allGoals.insert(allGoals.end(), transitions.begin(),
                         transitions.end());
               const auto & hlines = sub->GetAllHlines();
               allGoals.insert(allGoals.end(), hlines.begin(), hlines.end());

               //process the hlines
               //process the crossings
               //process the transitions
               for (unsigned int n1 = 0; n1 < allGoals.size(); n1++)
               {
                    NavLine* nav1 = allGoals[n1];
                    AccessPoint* from_AP = _accessPoints[nav1->GetUniqueID()];
                    int from_door = _map_id_to_index[nav1->GetUniqueID()];
                    if(from_AP->IsClosed()) continue;

                    for (unsigned int n2 = 0; n2 < allGoals.size(); n2++) {
                         NavLine* nav2 = allGoals[n2];
                         AccessPoint* to_AP = _accessPoints[nav2->GetUniqueID()];
                         if(to_AP->IsClosed()) continue;

                         if (n1 == n2)
                              continue;
                         if (nav1->operator ==(*nav2))
                              continue;

                         if (building->IsVisible(nav1->GetCentre(), nav2->GetCentre(), true)) {
                              int to_door = _map_id_to_index[nav2->GetUniqueID()];
                              _distMatrix[from_door][to_door] = penalty*(nav1->GetCentre()
                                        - nav2->GetCentre()).Norm();
                              from_AP->AddConnectingAP(
                                        _accessPoints[nav2->GetUniqueID()]);
                         }
                    }
               }
          }
     }

     //complete the matrix with the final distances between the exits to the outside and the
     //final marked goals

     for (int final_dest:_finalDestinations)
     {
          Goal* goal =_building->GetFinalGoal(final_dest);
          const Wall& line=_building->GetFinalGoal(final_dest)->GetAllWalls()[0];
          double center[2] = { goal->GetCentroid()._x, goal->GetCentroid()._y };

          AccessPoint* to_AP = new AccessPoint(line.GetUniqueID(), center);
          to_AP->SetFinalGoalOutside(true);
          to_AP->SetNavLine(new NavLine(line));
          char friendlyName[CLENGTH];
          sprintf(friendlyName, "finalGoal_%d_located_outside", goal->GetId());
          to_AP->SetFriendlyName(friendlyName);
          to_AP->AddFinalDestination(FINAL_DEST_OUT,0.0);
          to_AP->AddFinalDestination(goal->GetId(),0.0);
          _accessPoints[to_AP->GetID()] = to_AP;

          //very nasty
          _map_id_to_index[to_AP->GetID()] = index;
          _map_index_to_id[index] = to_AP->GetID();
          index++;

          //only make a connection to final exit to outside
          for(const auto & itr1: _accessPoints)
          {
               AccessPoint* from_AP = itr1.second;
               if(from_AP->GetFinalExitToOutside()==false) continue;
               if(from_AP->GetID()==to_AP->GetID()) continue;
               from_AP->AddConnectingAP(to_AP);
               int from_door= _map_id_to_index[from_AP->GetID()];
               int to_door= _map_id_to_index[to_AP->GetID()];
               // I assume a direct line connection between every exit connected to the outside and
               // any final goal also located outside
               _distMatrix[from_door][to_door] = _edgeCost*from_AP->GetNavLine()->DistTo(goal->GetCentroid());

               // add a penalty for goals outside due to the direct line assumption while computing the distances
               //if (_distMatrix[from_door][to_door] > 10.0)
               //      _distMatrix[from_door][to_door]*=10;
          }
     }

     //run the floyd warshall algorithm
     FloydWarshall();

     // set the configuration for reaching the outside
     // set the distances to all final APs

     for(const auto & itr: _accessPoints)
     {
          AccessPoint* from_AP = itr.second;
          int from_door = _map_id_to_index[itr.first];
          if(from_AP->GetFinalGoalOutside()) continue;

          //maybe put the distance to FLT_MAX
          if(from_AP->IsClosed()) continue;

          double tmpMinDist = FLT_MAX;
          int tmpFinalGlobalNearestID = from_door;

          for(const auto & itr1: _accessPoints)
          {
               AccessPoint* to_AP = itr1.second;
               if(from_AP->GetID()==to_AP->GetID()) continue;
               if(from_AP->GetFinalExitToOutside()) continue;
               //if(from_AP->GetFinalGoalOutside()) continue;

               if (to_AP->GetFinalExitToOutside())
               {
                    int to_door = _map_id_to_index[itr1.first];
                    if (from_door == to_door)
                         continue;

                    //cout <<" checking final destination: "<< pAccessPoints[j]->GetID()<<endl;
                    double dist = _distMatrix[from_door][to_door];
                    if (dist < tmpMinDist) {
                         tmpFinalGlobalNearestID = to_door;
                         tmpMinDist = dist;
                    }
               }
          }

          // in the case it is the final APs
          if (tmpFinalGlobalNearestID == from_door)
               tmpMinDist = 0.0;

          if (tmpMinDist == FLT_MAX) {
               Log->Write(
                         "ERROR: \tGlobalRouter: There is no visibility path from [%s] to the outside 1\n"
                         "       You can solve this by enabling triangulation.",
                         from_AP->GetFriendlyName().c_str());
               from_AP->Dump();
               return false;
          }

          // set the distance to the final destination ( OUT )
          from_AP->AddFinalDestination(FINAL_DEST_OUT, tmpMinDist);

          // set the intermediate path to global final destination
          GetPath(from_door, tmpFinalGlobalNearestID);

          if (_tmpPedPath.size() >= 2) {
               from_AP->AddTransitAPsTo(FINAL_DEST_OUT,
                         _accessPoints[_map_index_to_id[_tmpPedPath[1]]]);
          } else {
               if ((!from_AP->GetFinalExitToOutside())
                         && (!from_AP->IsClosed()))
               {
                    Log->Write(
                              "ERROR: \tGlobalRouter: There is no visibility path from [%s] to the outside 2\n"
                              "       \tYou can solve this by enabling triangulation.",
                              from_AP->GetFriendlyName().c_str());
                    from_AP->Dump();
                    return false;
               }
          }
          _tmpPedPath.clear();
     }

     // set the configuration to reach the goals specified in the ini file
     // set the distances to alternative destinations

     for (unsigned int p = 0; p < _finalDestinations.size(); p++) {
          int to_door_uid =
                    _building->GetFinalGoal(_finalDestinations[p])->GetAllWalls()[0].GetUniqueID();
          int to_door_matrix_index=_map_id_to_index[to_door_uid];

          // thats probably a goal located outside the geometry or not an exit from the geometry
          if(to_door_uid==-1) {
               Log->Write(
                         "ERROR: \tGlobalRouter: there is something wrong with the final destination [ %d ]\n",
                         _finalDestinations[p]);
               return false;
          }

          for(const auto & itr:_accessPoints)
          {
               AccessPoint* from_AP = itr.second;
               if(from_AP->GetFinalGoalOutside()) continue;
               if(from_AP->IsClosed()) continue;
               int from_door_matrix_index = _map_id_to_index[itr.first];

               //comment this if you want infinite as distance to unreachable destinations
               double dist = _distMatrix[from_door_matrix_index][to_door_matrix_index];
               from_AP->AddFinalDestination(_finalDestinations[p], dist);

               // set the intermediate path
               // set the intermediate path to global final destination
               GetPath(from_door_matrix_index, to_door_matrix_index);
               if (_tmpPedPath.size() >= 2) {
                    from_AP->AddTransitAPsTo(_finalDestinations[p],
                              _accessPoints[_map_index_to_id[_tmpPedPath[1]]]);
               } else {
                    if (((!from_AP->IsClosed()))) {
                         Log->Write(
                                   "ERROR: \tGlobalRouter: There is no visibility path from [%s] to goal [%d]\n"
                                   "         You can solve this by enabling triangulation.",
                                   from_AP->GetFriendlyName().c_str(), _finalDestinations[p]);
                         from_AP->Dump();
                         return false;
                    }
               }
               _tmpPedPath.clear();
          }
     }

     //dumping the complete system
     //DumpAccessPoints(-1); //exit(0);
     //DumpAccessPoints(-1); exit(0);
     //vector<string> rooms;
     //rooms.push_back("hall");
     //WriteGraphGV("routing_graph.gv",FINAL_DEST_OUT,rooms); exit(0);
     //WriteGraphGV("routing_graph.gv",1,rooms);
     Log->Write("INFO:\tDone with the Global Router Engine!");
     return true;
}

void GlobalRouter::Reset(){
     //clean all allocated spaces
     if (_distMatrix && _pathsMatrix) {
          const int exitsCnt = _building->GetNumberOfGoals();
          for (int p = 0; p < exitsCnt; ++p) {
               delete[] _distMatrix[p];
               delete[] _pathsMatrix[p];
          }

          delete[] _distMatrix;
          delete[] _pathsMatrix;
     }

     for (auto itr = _accessPoints.begin(); itr != _accessPoints.end(); ++itr) {
          delete itr->second;
     }

     _accessPoints.clear();
     _tmpPedPath.clear();
     _map_id_to_index.clear();
     _map_index_to_id.clear();
     _mapIdToFinalDestination.clear();
}

void GlobalRouter::SetEdgeCost(double cost)
{
     _edgeCost=cost;
}

double GlobalRouter::GetEdgeCost() const
{
     return _edgeCost;
}

void GlobalRouter::GetPath(int i, int j)
{
     if (_distMatrix[i][j] == FLT_MAX)
          return;
     if (i != j)
          GetPath(i, _pathsMatrix[i][j]);
     _tmpPedPath.push_back(j);
}

bool GlobalRouter::GetPath(Pedestrian* ped, std::vector<NavLine*>& path)
{
     std::vector<AccessPoint*> aps_path;

     bool done=false;
     int currentNavLine = ped->GetNextDestination();
     if (currentNavLine == -1)
     {
          currentNavLine= GetBestDefaultRandomExit(ped);
     }
     aps_path.push_back(_accessPoints[currentNavLine]);

     int loop_count=1;
     do
     {
          const auto & ap=aps_path.back();
          int next_dest = ap->GetNearestTransitAPTO(ped->GetFinalDestination());

          if(next_dest==-1) break; //we are done

          auto & next_ap= _accessPoints[next_dest];

          if(next_ap->GetFinalExitToOutside())
          {
               done =true;
          }

          if (! IsElementInVector(aps_path,next_ap))
          {
               aps_path.push_back(next_ap);
          }
          else
          {
               Log->Write("WARNING:\t the line [%d] is already included in the path.");
          }

          //work arround to detect a potential infinte loop.
          if(loop_count++>1000)
          {
               Log->Write("ERROR:\t A path could not be found for pedestrian [%d] going to destination [%d]",ped->GetID(),ped->GetFinalDestination());
               Log->Write("      \t Stuck in an infinite loop [%d].",loop_count);
               return false;
          }


     } while (!done);


     for(const auto & aps: aps_path)
          path.push_back(aps->GetNavLine());

     return true;

     //collect and return the navigation lines

     //     do
     //     {
     //          SubRoom* sub = _building->GetRoom(ped->GetRoomID())->GetSubRoom(
     //                    ped->GetSubRoomID());
     //
     //          const vector<int>& accessPointsInSubRoom = sub->GetAllGoalIDs();
     //          for (unsigned int i = 0; i < accessPointsInSubRoom.size(); i++) {
     //
     //               int apID = accessPointsInSubRoom[i];
     //               AccessPoint* ap = _accessPoints[apID];
     //
     //               const Point& pt3 = ped->GetPos();
     //               double distToExit = ap->GetNavLine()->DistTo(pt3);
     //
     //               if (distToExit > J_EPS_DIST)
     //                    continue;
     //
     //               //one AP is near actualize destination:
     //               nextDestination = ap->GetNearestTransitAPTO(
     //                         ped->GetFinalDestination());
     //
     //
     //               if (nextDestination == -1) { // we are almost at the exit
     //                    return ped->GetNextDestination();
     //               } else {
     //                    //check that the next destination is in the actual room of the pedestrian
     //                    if (_accessPoints[nextDestination]->isInRange(
     //                              sub->GetUID())==false) {
     //                         //return the last destination if defined
     //                         int previousDestination = ped->GetNextDestination();
     //
     //                         //we are still somewhere in the initialization phase
     //                         if (previousDestination == -1) {
     //                              ped->SetExitIndex(apID);
     //                              ped->SetExitLine(_accessPoints[apID]->GetNavLine());
     //                              return apID;
     //                         } else { // we are still having a valid destination, don't change
     //                              return previousDestination;
     //                         }
     //                    } else { // we have reached the new room
     //                         ped->SetExitIndex(nextDestination);
     //                         ped->SetExitLine(
     //                                   _accessPoints[nextDestination]->GetNavLine());
     //                         return nextDestination;
     //                    }
     //               }
     //          }
     //
     //          // still have a valid destination, so return it
     //          return nextDestination;
     //     }while (!done);
}

bool GlobalRouter::GetPath(Pedestrian*ped, int goalID, std::vector<SubRoom*>& path)
{

     //clear the global variable holding the paths
     _tmpPedPath.clear();

     int tmpFinalDest=ped->GetFinalDestination();
     ped->SetFinalDestination(goalID);

     //find the nearest APs and start from there
     int next = GetBestDefaultRandomExit(ped);
     if(next==-1) {
          Log->Write("ERROR:\t there is an error in getting the path for ped %d to the goal %d", ped->GetID(),goalID);
          return false;
     }

     // get the transformed goal_id
     int to_door_uid =
               _building->GetFinalGoal(goalID)->GetAllWalls()[0].GetUniqueID();
     int to_door_matrix_index=_map_id_to_index[to_door_uid];
     int from_door_matrix_index=_map_id_to_index[next];

     // thats probably a goal located outside the geometry or not an exit from the geometry
     if(to_door_uid==-1) {
          Log->Write("ERROR: \tGlobalRouter: there is something wrong with final destination [ %d ]\n",goalID);
          return false;
     }

     //populate the line unique id to cross
     GetPath(from_door_matrix_index,to_door_matrix_index);

     for(unsigned int i=0; i<_tmpPedPath.size(); i++) {
          int ap_id= _map_index_to_id[_tmpPedPath[i]];
          int subroom_uid=_accessPoints[ap_id]->GetConnectingRoom1();
          if(subroom_uid==-1) continue;
          SubRoom* sub = _building->GetSubRoomByUID(subroom_uid);
          if (sub && IsElementInVector(path, sub)==false) path.push_back(sub);
     }

     for(unsigned int i=0; i<_tmpPedPath.size(); i++) {
          int ap_id= _map_index_to_id[_tmpPedPath[i]];
          int subroom_uid=_accessPoints[ap_id]->GetConnectingRoom2();
          if(subroom_uid==-1) continue;
          SubRoom* sub = _building->GetSubRoomByUID(subroom_uid);
          if (sub && IsElementInVector(path, sub)==false) path.push_back(sub);
     }

     //clear the global variable holding the paths
     _tmpPedPath.clear();

     ped->SetFinalDestination(tmpFinalDest);
     return true;
     //double distance = _accessPoints[next]->GetDistanceTo(0)+ped->GetDistanceToNextTarget();
     //cout<<"shortest distance to outside: " <<distance<<endl;
}

/*
 floyd_warshall()
 after calling this function dist[i][j] will the the minimum distance
 between i and j if it exists (i.e. if there's a path between i and j)
 or 0, otherwise
 */
void GlobalRouter::FloydWarshall()
{
     const int n = _building->GetNumberOfGoals() + _building->GetAllGoals().size();
     for (int k = 0; k < n; k++)
          for (int i = 0; i < n; i++)
               for (int j = 0; j < n; j++)
                    if (_distMatrix[i][k] + _distMatrix[k][j] < _distMatrix[i][j]) {
                         _distMatrix[i][j] = _distMatrix[i][k] + _distMatrix[k][j];
                         _pathsMatrix[i][j] = _pathsMatrix[k][j];
                    }
}

void GlobalRouter::DumpAccessPoints(int p)
{
     if (p != -1) {
          _accessPoints.at(p)->Dump();
     } else {
          for (const auto & itr: _accessPoints)
          {
               itr.second->Dump();
          }
     }
}

int GlobalRouter::FindExit(Pedestrian* ped)
{
     if(_useMeshForLocalNavigation==false)
     {
          std::vector<NavLine*> path;
          GetPath(ped,path);

          //return the next path which is an exit
          for(const auto & navLine: path)
          {
               //TODO: only set if the pedestrian is already in the subroom.
               // cuz all lines are returned
               if(IsCrossing(*navLine) || IsTransition(*navLine))
               {
                    int nav_id= navLine->GetUniqueID();
                    ped->SetExitIndex(nav_id);
                    ped->SetExitLine(navLine);
                    return nav_id;
               }
          }

          //something bad happens
          Log->Write(
                    "ERROR:\t Cannot find a valid destination for ped [%d] located in room [%d] subroom [%d] going to destination [%d]",
                    ped->GetID(), ped->GetRoomID(), ped->GetSubRoomID(),
                    ped->GetFinalDestination());
          return -1;

     }

     // else proceed as usual and return the closest navigation line

     int nextDestination = ped->GetNextDestination();
     //      if(ped->GetGlobalTime()>80){
     //              ped->Dump(2);
     //              //exit(0);
     //      }

     if (nextDestination == -1) {
          return GetBestDefaultRandomExit(ped);

     } else {

          SubRoom* sub = _building->GetRoom(ped->GetRoomID())->GetSubRoom(
                    ped->GetSubRoomID());

          for(const auto & apID:sub->GetAllGoalIDs())
          {
               AccessPoint* ap = _accessPoints[apID];
               const Point& pt3 = ped->GetPos();
               double distToExit = ap->GetNavLine()->DistTo(pt3);

               if (distToExit > J_EPS_DIST)
                    continue;

               //one AP is near actualize destination:
               nextDestination = ap->GetNearestTransitAPTO(
                         ped->GetFinalDestination());


               if (nextDestination == -1) { // we are almost at the exit
                    return ped->GetNextDestination();
               } else {
                    //check that the next destination is in the actual room of the pedestrian
                    if (_accessPoints[nextDestination]->isInRange(
                              sub->GetUID())==false) {
                         //return the last destination if defined
                         int previousDestination = ped->GetNextDestination();

                         //we are still somewhere in the initialization phase
                         if (previousDestination == -1) {
                              ped->SetExitIndex(apID);
                              ped->SetExitLine(_accessPoints[apID]->GetNavLine());
                              return apID;
                         } else { // we are still having a valid destination, don't change
                              return previousDestination;
                         }
                    } else { // we have reached the new room
                         ped->SetExitIndex(nextDestination);
                         ped->SetExitLine(
                                   _accessPoints[nextDestination]->GetNavLine());
                         return nextDestination;
                    }
               }
          }

          // still have a valid destination, so return it
          return nextDestination;
     }
}

int GlobalRouter::GetBestDefaultRandomExit(Pedestrian* ped)
{
     // prob parameters
     //double alpha=0.2000005;
     //double normFactor=0.0;
     //map <int, double> doorProb;

     // get the opened exits
     SubRoom* sub = _building->GetRoom(ped->GetRoomID())->GetSubRoom(
               ped->GetSubRoomID());


     // get the relevant opened exits
     vector <AccessPoint*> relevantAPs;
     GetRelevantRoutesTofinalDestination(ped,relevantAPs);
     //cout<<"relevant APs size:" <<relevantAPs.size()<<endl;

     int bestAPsID = -1;
     double minDistGlobal = FLT_MAX;
     double minDistLocal = FLT_MAX;

     //for (unsigned int i = 0; i < accessPointsInSubRoom.size(); i++) {
     //      int apID = accessPointsInSubRoom[i];
     for(unsigned int g=0; g<relevantAPs.size(); g++) {
          AccessPoint* ap=relevantAPs[g];
          //int exitid=ap->GetID();
          //AccessPoint* ap = _accessPoints[apID];

          if (ap->isInRange(sub->GetUID()) == false)
               continue;
          //check if that exit is open.
          if (ap->IsClosed())
               continue;

          //the line from the current position to the centre of the nav line.
          // at least the line in that direction minus EPS
          const Point& posA = ped->GetPos();
          const Point& posB = ap->GetNavLine()->GetCentre();
          const Point& posC = (posB - posA).Normalized() * ((posA - posB).Norm() - J_EPS) + posA;

          //check if visible
          if (sub->IsVisible(posA, posC, true) == false) {
               ped->RerouteIn(10);
               //ped->Dump(ped->GetID());
               continue;
          }

          double dist1 = ap->GetDistanceTo(ped->GetFinalDestination());
          double dist2 = ap->DistanceTo(posA.GetX(), posA.GetY());
          double dist=dist1+dist2;

          //        doorProb[ap->GetID()]= exp(-alpha*dist);
          //        normFactor += doorProb[ap->GetID()];


          //          if (dist < minDistGlobal) {
          //               bestAPsID = ap->GetID();
          //               minDistGlobal = dist;
          //          }

          // normalize the probs
          //    double randomVar = _rdDistribution(_rdGenerator);
          //
          //    for (const auto & it = doorProb.begin(); it!=doorProb.end(); ++it){
          //        it->second =  it->second / normFactor;
          //    }
          //
          //    double cumProb= doorProb.begin()->second;
          //    const auto & it = doorProb.begin();
          //    while(cumProb<randomVar) {
          //        it++;
          //        cumProb+=it->second;
          //    }
          //    bestAPsID=it->first;

          //very useful for short term decisions
          // if two doors are feasible to the final destination without much differences
          // in the distances, then the nearest is preferred.
          if(( (dist-minDistGlobal) / (dist+minDistGlobal)) < CBA_THRESHOLD)
          {
               if (dist2 < minDistLocal) {
                    bestAPsID = ap->GetID();
                    minDistGlobal = dist;
                    minDistLocal= dist2;
               }

          } else {

               if (dist < minDistGlobal) {
                    bestAPsID = ap->GetID();
                    minDistGlobal = dist;
                    minDistLocal=dist2;
               }
          }
     }

     if (bestAPsID != -1) {
          ped->SetExitIndex(bestAPsID);
          ped->SetExitLine(_accessPoints[bestAPsID]->GetNavLine());
          return bestAPsID;
     } else {
          if (_building->GetRoom(ped->GetRoomID())->GetCaption() != "outside")
               Log->Write(
                         "ERROR:\t GetBestDefaultRandomExit() \nCannot find valid destination for ped [%d] "
                         "located in room [%d] subroom [%d] going to destination [%d]",
                         ped->GetID(), ped->GetRoomID(), ped->GetSubRoomID(),
                         ped->GetFinalDestination());
          return -1;
     }
}


void GlobalRouter::GetRelevantRoutesTofinalDestination(Pedestrian *ped, vector<AccessPoint*>& relevantAPS)
{

     Room* room=_building->GetRoom(ped->GetRoomID());
     SubRoom* sub=room->GetSubRoom(ped->GetSubRoomID());

     // This is best implemented by closing one door and checking if there is still a path to outside
     // and itereating over the others.
     // It might be time consuming, you many pre compute and cache the results.
     if(sub->GetAllHlines().size()==0)
     {
          const vector<int>& goals=sub->GetAllGoalIDs();
          //filter to keep only the emergencies exits.

          for(unsigned int g1=0; g1<goals.size(); g1++) {
               AccessPoint* ap=_accessPoints[goals[g1]];
               bool relevant=true;
               for(unsigned int g2=0; g2<goals.size(); g2++) {
                    if(goals[g2]==goals[g1]) continue; // always skip myself
                    if(ap->GetNearestTransitAPTO(ped->GetFinalDestination())==goals[g2]) {
                         // crossings only
                         relevant=false;
                         break;
                    }
               }
               if(relevant==true) {
                    //only if not closed
                    if(ap->IsClosed()==false)
                         relevantAPS.push_back(ap);
                    //cout<<"relevant APs:" <<ap->GetID()<<endl;
               }
          }

     }
     //quick fix for extra hlines
     // it should be safe now to delete the first preceding if block
     else
     {
          const vector<int>& goals=sub->GetAllGoalIDs();
          for(unsigned int g1=0; g1<goals.size(); g1++)
          {
               AccessPoint* ap=_accessPoints[goals[g1]];

               //check for visibility
               //the line from the current position to the centre of the nav line.
               // at least the line in that direction minus EPS
               const Point& posA = ped->GetPos();
               const Point& posB = ap->GetNavLine()->GetCentre();
               const Point& posC = (posB - posA).Normalized() * ((posA - posB).Norm() - J_EPS) + posA;

               //check if visible
               if (sub->IsVisible(posA, posC, true) == false)
               {
                    continue;
               }

               bool relevant=true;
               for(unsigned int g2=0; g2<goals.size(); g2++)
               {
                    if(goals[g2]==goals[g1]) continue; // always skip myself
                    if(ap->GetNearestTransitAPTO(ped->GetFinalDestination())==goals[g2])
                    {

                         //pointing only to the one i dont see
                         //the line from the current position to the centre of the nav line.
                         // at least the line in that direction minus EPS
                         AccessPoint* ap2=_accessPoints[goals[g2]];
                         const Point& posA = ped->GetPos();
                         const Point& posB = ap2->GetNavLine()->GetCentre();
                         const Point& posC = (posB - posA).Normalized()* ((posA - posB).Norm() - J_EPS) + posA;

                         //it points to a destination that I can see anyway
                         if (sub->IsVisible(posA, posC, true) == true)
                         {
                              relevant=false;
                         }

                         break;
                    }
               }
               if(relevant==true)
               {
                    if(ap->IsClosed()==false)
                         relevantAPS.push_back(ap);
                    //cout<<"relevant APs:" <<ap->GetID()<<endl;
               }
          }
     }
     //    if(relevantAPS.size()==2){
     //         cout<<"alternative wege: "<<relevantAPS.size()<<endl;
     //         cout<<"ap1: "<<relevantAPS[0]->GetID()<<endl;
     //         cout<<"ap2: "<<relevantAPS[1]->GetID()<<endl;
     //         getc(stdin);
     //    }
}

SubRoom* GlobalRouter::GetCommonSubRoom(Crossing* c1, Crossing* c2)
{
     SubRoom* sb11 = c1->GetSubRoom1();
     SubRoom* sb12 = c1->GetSubRoom2();
     SubRoom* sb21 = c2->GetSubRoom1();
     SubRoom* sb22 = c2->GetSubRoom2();

     if (sb11 == sb21)
          return sb11;
     if (sb11 == sb22)
          return sb11;
     if (sb12 == sb21)
          return sb12;
     if (sb12 == sb22)
          return sb12;

     return NULL;
}

void GlobalRouter::WriteGraphGV(string filename, int finalDestination,
          const vector<string> rooms_captions)
{
     ofstream graph_file(filename.c_str());
     if (graph_file.is_open() == false) {
          Log->Write("Unable to open file" + filename);
          return;
     }

     //header
     graph_file << "## Produced by OPS_GCFM" << endl;
     //graph_file << "##comand: \" sfdp -Goverlap=prism -Gcharset=latin1"<<filename <<"| gvmap -e | neato -Ecolor=\"#55555522\" -n2 -Tpng > "<< filename<<".png \""<<endl;
     graph_file << "##Command to produce the output: \"neato -n -s -Tpng "
               << filename << " > " << filename << ".png\"" << endl;
     graph_file << "digraph OPS_GCFM_ROUTING {" << endl;
     graph_file << "overlap=scale;" << endl;
     graph_file << "splines=false;" << endl;
     graph_file << "fontsize=20;" << endl;
     graph_file
     << "label=\"Graph generated by the routing engine for destination: "
     << finalDestination << "\"" << endl;

     vector<int> rooms_ids = vector<int>();

     if (rooms_captions.empty()) {
          // then all rooms should be printed
          for (int i = 0; i < _building->GetNumberOfRooms(); i++) {
               rooms_ids.push_back(i);
          }

     } else {
          for (unsigned int i = 0; i < rooms_captions.size(); i++) {
               rooms_ids.push_back(
                         _building->GetRoom(rooms_captions[i])->GetID());
          }
     }

     for (map<int, AccessPoint*>::const_iterator itr = _accessPoints.begin();
               itr != _accessPoints.end(); ++itr) {

          AccessPoint* from_AP = itr->second;

          int from_door = from_AP->GetID();

          // check for valid room
          NavLine* nav = from_AP->GetNavLine();
          int room_id = -1;

          if (dynamic_cast<Crossing*>(nav) != NULL) {
               room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

          } else if (dynamic_cast<Hline*>(nav) != NULL) {
               room_id = ((Hline*) (nav))->GetRoom1()->GetID();

          } else if (dynamic_cast<Transition*>(nav) != NULL) {
               room_id = ((Transition*) (nav))->GetRoom1()->GetID();

          } else {
               cout << "WARNING: Unkown navigation line type" << endl;
               continue;
          }

          if (IsElementInVector(rooms_ids, room_id) == false)
               continue;

          double px = from_AP->GetCentre().GetX();
          double py = from_AP->GetCentre().GetY();
          //graph_file << from_door <<" [shape=ellipse, pos=\""<<px<<", "<<py<<" \"] ;"<<endl;
          //graph_file << from_door <<" [shape=ellipse, pos=\""<<px<<","<<py<<"\" ];"<<endl;

          //const vector<AccessPoint*>& from_aps = from_AP->GetConnectingAPs();
          const vector<AccessPoint*>& from_aps = from_AP->GetTransitAPsTo(
                    finalDestination);

          if (from_aps.size() == 0) {

               if (from_AP->GetFinalExitToOutside()) {
                    graph_file << from_door << " [pos=\"" << px << ", " << py
                              << " \", style=filled, color=green,fontsize=5] ;"
                              << endl;
                    //                              graph_file << from_door <<" [width=\"0.41\", height=\"0.31\",fixedsize=false,pos=\""<<px<<", "<<py<<" \", style=filled, color=green,fontsize=4] ;"<<endl;
               } else {
                    graph_file << from_door << " [pos=\"" << px << ", " << py
                              << " \", style=filled, color=red,fontsize=5] ;" << endl;
                    //                              graph_file << from_door <<" [width=\"0.41\", height=\"0.31\",fixedsize=false,pos=\""<<px<<", "<<py<<" \", style=filled, color=red,fontsize=4] ;"<<endl;
               }
          } else {
               // check that all connecting aps are contained in the room_ids list
               // if not marked as sink.
               bool isSink = true;
               for (unsigned int j = 0; j < from_aps.size(); j++) {
                    NavLine* nav = from_aps[j]->GetNavLine();
                    int room_id = -1;

                    if (dynamic_cast<Crossing*>(nav) != NULL) {
                         room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

                    } else if (dynamic_cast<Hline*>(nav) != NULL) {
                         room_id = ((Hline*) (nav))->GetRoom1()->GetID();

                    } else if (dynamic_cast<Transition*>(nav) != NULL) {
                         room_id = ((Transition*) (nav))->GetRoom1()->GetID();

                    } else {
                         cout << "WARNING: Unkown navigation line type" << endl;
                         continue;
                    }

                    if (IsElementInVector(rooms_ids, room_id) == true) {
                         isSink = false;
                         break;
                    }
               }

               if (isSink) {
                    //graph_file << from_door <<" [width=\"0.3\", height=\"0.21\",fixedsize=false,pos=\""<<px<<", "<<py<<" \" ,style=filled, color=green, fontsize=4] ;"<<endl;
                    graph_file << from_door << " [pos=\"" << px << ", " << py
                              << " \" ,style=filled, color=blue, fontsize=5] ;"
                              << endl;
               } else {
                    //graph_file << from_door <<" [width=\"0.3\", height=\"0.231\",fixedsize=false, pos=\""<<px<<", "<<py<<" \", fontsize=4] ;"<<endl;
                    graph_file << from_door << " [pos=\"" << px << ", " << py
                              << " \", style=filled, color=yellow, fontsize=5] ;"
                              << endl;
               }
          }

     }

     //connections
     for (const auto & itr: _accessPoints)
     {
          AccessPoint* from_AP = itr.second;
          int from_door = from_AP->GetID();



          NavLine* nav = from_AP->GetNavLine();
          int room_id = -1;

          if (dynamic_cast<Crossing*>(nav) != NULL) {
               room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

          } else if (dynamic_cast<Hline*>(nav) != NULL) {
               room_id = ((Hline*) (nav))->GetRoom1()->GetID();

          } else if (dynamic_cast<Transition*>(nav) != NULL) {
               room_id = ((Transition*) (nav))->GetRoom1()->GetID();

          } else {
               cout << "WARNING: Unkown navigation line type" << endl;
               continue;
          }

          if (IsElementInVector(rooms_ids, room_id) == false)
               continue;

          //const vector<AccessPoint*>& aps = from_AP->GetConnectingAPs();
          const vector<AccessPoint*>& aps = from_AP->GetTransitAPsTo(
                    finalDestination);

          for (const auto & to_AP:aps)
          {
               int to_door = to_AP->GetID();

               NavLine* nav = to_AP->GetNavLine();
               int room_id = -1;

               if (dynamic_cast<Crossing*>(nav) != NULL) {
                    room_id = ((Crossing*) (nav))->GetRoom1()->GetID();

               } else if (dynamic_cast<Hline*>(nav) != NULL) {
                    room_id = ((Hline*) (nav))->GetRoom1()->GetID();

               } else if (dynamic_cast<Transition*>(nav) != NULL) {
                    room_id = ((Transition*) (nav))->GetRoom1()->GetID();

               } else {
                    cout << "WARNING: Unkown navigation line type" << endl;
                    continue;
               }

               if (IsElementInVector(rooms_ids, room_id) == false)
                    continue;

               graph_file << from_door << " -> " << to_door << " [ label="
                         << from_AP->GetDistanceTo(to_AP)
                         + to_AP->GetDistanceTo(finalDestination)
                         << ", fontsize=10]; " << endl;
          }
     }

     //graph_file << "node [shape=box];  gy2; yr2; rg2; gy1; yr1; rg1;"<<endl;
     //graph_file << "node [shape=circle,fixedsize=true,width=0.9];  green2; yellow2; red2; safe2; safe1; green1; yellow1; red1;"<<endl;

     //graph_file << "0 -> 1 ;"<<endl;

     graph_file << "}" << endl;

     //done
     graph_file.close();
}

void GlobalRouter::TriangulateGeometry()
{
     Log->Write("INFO:\tTriangulating the geometry");
     for(auto&& itr_room: _building->GetAllRooms())
     {
          for(auto&& itr_subroom: itr_room.second->GetAllSubRooms())
          {
               auto&& subroom=itr_subroom.second;
               auto&& room=itr_room.second;
               auto&& obstacles=subroom->GetAllObstacles();

               //Triangulate if obstacle or concave and no hlines ?
               //if(subroom->GetAllHlines().size()==0)
               if((obstacles.size()>0 ) || (subroom->IsConvex()==false ))
               {
                    DTriangulation* tri= new DTriangulation();

                    auto outerhull=subroom->GetPolygon();
                    if(subroom->IsClockwise())
                         std::reverse(outerhull.begin(), outerhull.end());

                    tri->SetOuterPolygone(outerhull);

                    for (const auto & obst: obstacles)
                    {
                         auto outerhullObst=obst->GetPolygon();
                         if(obst->IsClockwise())
                              std::reverse(outerhullObst.begin(), outerhullObst.end());
                         tri->AddHole(outerhullObst);
                    }

                    tri->Triangulate();
                    vector<p2t::Triangle*> triangles=tri->GetTriangles();

                    for (const auto & tr: triangles)
                    {
                         Point P0  = Point (tr->GetPoint(0)->x,tr->GetPoint(0)->y);
                         Point P1  = Point (tr->GetPoint(1)->x,tr->GetPoint(1)->y);
                         Point P2  = Point (tr->GetPoint(2)->x,tr->GetPoint(2)->y);
                         vector<Line> edges;
                         edges.push_back(Line(P0,P1));
                         edges.push_back(Line(P1,P2));
                         edges.push_back(Line(P2,P0));

                         for (const auto & line: edges)
                         {
                              if((IsWall(line)==false) && (IsCrossing(line)==false)
                                        && (IsTransition(line)==false) && (IsHline(line)==false))
                              {
                                   //add as a Hline
                                   int id=_building->GetAllHlines().size();
                                   Hline* h = new Hline();
                                   h->SetID(id);
                                   h->SetPoint1(line.GetPoint1());
                                   h->SetPoint2(line.GetPoint2());
                                   h->SetRoom1(room.get());
                                   h->SetSubRoom1(subroom.get());
                                   subroom->AddHline(h);
                                   _building->AddHline(h);
                              }
                         }
                    }
                    delete tri;
               }
          }
     }
     Log->Write("INFO:\tDone...");
}

bool GlobalRouter::GenerateNavigationMesh()
{
//     //Navigation mesh implementation
//     NavMesh* nv= new NavMesh(_building);
//     nv->BuildNavMesh();
//     _building->SaveGeometry("test_geometry.xml");
//     exit(0);
//     //nv->WriteToFileTraVisTo()
//
//     const std::vector<NavMesh::JEdge*>& edges = nv->GetEdges();
//
//     for(const auto & edge: edges)
//     {
//          //construct and add a new navigation line if non existing
//          Line line(edge->pStart.pPos,edge->pEnd.pPos);
//          bool isEdge=false;
//
//          //check if it is already a crossing
//          const map<int, Crossing*>& crossings = _building->GetAllCrossings();
//          for (const auto & crossing: crossings)
//          {
//               Crossing* cross=crossing.second;
//               if(line.operator ==(*cross))
//               {
//                    isEdge=true;
//                    break;
//               }
//          }
//          if(isEdge) continue;
//
//
//          //check if it is already a transition
//          const map<int, Transition*>& transitions = _building->GetAllTransitions();
//          for (const auto & transition: transitions)
//          {
//               Transition* trans=transition.second;
//               if(line.operator ==(*trans))
//               {
//                    isEdge=true;
//                    break;
//               }
//          }
//          if(isEdge) continue;
//
//          //check if it is already a
//          const map<int, Hline*>& hlines = _building->GetAllHlines();
//          for (const auto & hline: hlines)
//          {
//               Hline* navLine=hline.second;
//               if(line.operator ==(*navLine))
//               {
//                    isEdge=true;
//                    break;
//               }
//          }
//          if(isEdge) continue;
//
//
//          Hline* h = new Hline();
//          h->SetID(hlines.size());
//          int assigned=0;
//
//          //look for the room/subroom containing the new edge
//          const vector<Room*>& rooms=_building->GetAllRooms();
//          for(const auto & room: rooms)
//          {
//               const vector<SubRoom*>& subrooms= room->GetAllSubRooms();
//
//               for(const auto & subroom: subrooms)
//               {
//                    if(subroom->IsInSubRoom(line.GetCentre()))
//                    {
//                         h->SetRoom1(room);
//                         h->SetSubRoom1(subroom);
//                         assigned++;
//                    }
//               }
//          }
//
//          if(assigned!=1)
//          {
//               Log->Write("WARNING:\t a navigation line from the mesh was not correctly assigned");
//               return false;
//          }
//          //add the new edge as navigation line
//
//          h->SetPoint1(edge->pStart.pPos);
//          h->SetPoint2(edge->pEnd.pPos);
//          h->GetSubRoom1()->AddHline(h); //double linked ??
//          _building->AddHline(h);
//
//     }
//
//     //string geometry;
//     //nv->WriteToString(geometry);
//     //Write("<geometry>");
//     //Write(geometry);
//     //Write("</geometry>");
//     //nv->WriteToFile(building->GetProjectFilename()+".full.nav");
//
//     //cout<<"bye"<<endl;
//     delete nv;
     return true;
}

string GlobalRouter::GetRoutingInfoFile()
{

     TiXmlDocument doc(_building->GetProjectFilename());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \t GlobalRouter: could not parse the project file");
          return "";
     }

     // everything is fine. proceed with parsing
     TiXmlElement* xMainNode = doc.RootElement();
     TiXmlNode* xRouters=xMainNode->FirstChild("route_choice_models");

     string nav_line_file="";

     for(TiXmlElement* e = xRouters->FirstChildElement("router"); e;
               e = e->NextSiblingElement("router"))
     {

          string strategy=e->Attribute("description");

          if(strategy=="local_shortest")
          {
               if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
                    nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");
          }
          else if(strategy=="global_shortest")
          {
               if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
                    nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");

               TiXmlElement* para =e->FirstChild("parameters")->FirstChildElement("navigation_mesh");
               if (para)
               {
                    string local_planing=xmltoa(para->Attribute("use_for_local_planning"),"false");
                    if(local_planing=="true") {
                         _useMeshForLocalNavigation = 1;
                    }
                    else {
                         _useMeshForLocalNavigation = 0;
                    }

                    string method = xmltoa(para->Attribute("method"),"");
                    if(method=="triangulation")
                    {
                         _generateNavigationMesh=true;
                    }
                    else
                    {
                         Log->Write("WARNING:\t only triangulation is supported for the mesh. You supplied [%s]",method.c_str());
                    }

               }

          }
          else if(strategy=="global_safest")
          {
               if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
                    nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");
          }
          else if(strategy=="dynamic")
          {
               if (e->FirstChild("parameters")->FirstChildElement("navigation_lines"))
                    nav_line_file=e->FirstChild("parameters")->FirstChildElement("navigation_lines")->Attribute("file");
          }
     }

     if (nav_line_file == "")
          return nav_line_file;
     else
          return _building->GetProjectRootDir()+nav_line_file;
}


bool GlobalRouter::LoadRoutingInfos(const std::string &filename)
{
     if(filename=="") return true;

     Log->Write("INFO:\tLoading extra routing information for the global/quickest path router");
     Log->Write("INFO:\t  from the file "+filename);

     TiXmlDocument docRouting(filename);
     if (!docRouting.LoadFile()) {
          Log->Write("ERROR: \t%s", docRouting.ErrorDesc());
          Log->Write("ERROR: \t could not parse the routing file [%s]",filename.c_str());
          return false;
     }

     TiXmlElement* xRootNode = docRouting.RootElement();
     if( ! xRootNode ) {
          Log->Write("ERROR:\tRoot element does not exist");
          return false;
     }

     if( xRootNode->ValueStr () != "routing" ) {
          Log->Write("ERROR:\tRoot element value is not 'routing'.");
          return false;
     }

     string  version = xRootNode->Attribute("version");
     if (version != JPS_VERSION && version != JPS_OLD_VERSION) {
          Log->Write("ERROR: \tOnly version  %d.%d supported",JPS_VERSION_MAJOR,JPS_VERSION_MINOR);
          Log->Write("ERROR: \tparsing routing file failed!");
          return false;
     }

     for(TiXmlElement* xHlinesNode = xRootNode->FirstChildElement("Hlines"); xHlinesNode;
               xHlinesNode = xHlinesNode->NextSiblingElement("Hlines")) {


          for(TiXmlElement* hline = xHlinesNode->FirstChildElement("Hline"); hline;
                    hline = hline->NextSiblingElement("Hline")) {

               double id = xmltof(hline->Attribute("id"), -1);
               int room_id = xmltoi(hline->Attribute("room_id"), -1);
               int subroom_id = xmltoi(hline->Attribute("subroom_id"), -1);

               double x1 = xmltof(     hline->FirstChildElement("vertex")->Attribute("px"));
               double y1 = xmltof(     hline->FirstChildElement("vertex")->Attribute("py"));
               double x2 = xmltof(     hline->LastChild("vertex")->ToElement()->Attribute("px"));
               double y2 = xmltof(     hline->LastChild("vertex")->ToElement()->Attribute("py"));

               Room* room = _building->GetRoom(room_id);
               SubRoom* subroom = room->GetSubRoom(subroom_id);

               //new implementation
               Hline* h = new Hline();
               h->SetID(id);
               h->SetPoint1(Point(x1, y1));
               h->SetPoint2(Point(x2, y2));
               h->SetRoom1(room);
               h->SetSubRoom1(subroom);

               _building->AddHline(h);
               subroom->AddHline(h);
          }
     }
     Log->Write("INFO:\tDone with loading extra routing information");
     return true;
}

bool GlobalRouter::IsWall(const Line& line) const
{
     for(auto&& itr_room: _building->GetAllRooms())
     {
          for(auto&& itr_subroom: itr_room.second->GetAllSubRooms())
          {
               for (auto&& obst: itr_subroom.second->GetAllObstacles())
               {
                    for (auto&& wall:obst->GetAllWalls())
                    {
                         if(line.operator ==(wall))
                              return true;
                    }
               }
               for (auto&& wall:itr_subroom.second->GetAllWalls())
               {
                    if(line.operator ==(wall))
                         return true;
               }
          }
     }

     return false;
}

bool GlobalRouter::IsCrossing(const Line& line) const
{
     for (const auto & crossing : _building->GetAllCrossings())
     {
          if (crossing.second->operator ==(line))
               return true;
     }
     return false;
}

bool GlobalRouter::IsTransition(const Line& line) const
{
     for(const auto & transition: _building->GetAllTransitions())
     {
          if (transition.second->operator ==(line))
               return true;
     }
     return false;
}

bool GlobalRouter::IsHline(const Line& line) const
{
     for(const auto & hline: _building->GetAllHlines())
     {
          if (hline.second->operator ==(line))
               return true;
     }
     return false;
}
