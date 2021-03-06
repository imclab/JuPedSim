/**
 * \file        PedDistributor.cpp
 * \date        Oct 12, 2010
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
 *
 *
 **/
#include <cmath>
#include "PedDistributor.h"
#include "../tinyxml/tinyxml.h"
#include "../geometry/Obstacle.h"
#include "../routing/RoutingEngine.h"
#include "../pedestrian/Pedestrian.h"
#include "../geometry/SubRoom.h"
#include "../IO/OutputHandler.h"
// #include "../logging.h"
using namespace std;


/************************************************************
 StartDistributionRoom
 ************************************************************/
StartDistributionRoom::StartDistributionRoom(int seed)
{
    _roomID = -1;
    _nPeds = -1;
    _groupID = -1;
    _goalID = -1;
    _routerID = -1;
    _routeID = -1;
    _age = -1;
    _height = -1;
    _startX = NAN;
    _startY = NAN;
    _startZ = NAN;
    _gender = "male";
    _patience=5;
    _xMin=-FLT_MAX;
    _xMax=FLT_MAX;
    _yMin=-FLT_MAX;
    _yMax=FLT_MAX;
    _groupParameters=NULL;
    _generator = std::default_random_engine(seed);
}

StartDistributionRoom::~StartDistributionRoom()
{
}


int StartDistributionRoom::GetAgentsNumber() const
{
     return _nPeds;
}


// Setter-Funktionen

void StartDistributionRoom::SetRoomID(int id)
{
     _roomID = id;
}

int StartDistributionRoom::GetAge() const
{
     return _age;
}

void StartDistributionRoom::SetAge(int age)
{
     _age = age;
}

const std::string& StartDistributionRoom::GetGender() const
{
     return _gender;
}

void StartDistributionRoom::SetGender(const std::string& gender)
{
     _gender = gender;
}

int StartDistributionRoom::GetGoalId() const
{
     return _goalID;
}

void StartDistributionRoom::SetGoalId(int goalId)
{
     _goalID = goalId;
}

int StartDistributionRoom::GetGroupId() const
{
     return _groupID;
}

void StartDistributionRoom::SetGroupId(int groupId)
{
     _groupID = groupId;
}

int StartDistributionRoom::GetHeight() const
{
     return _height;
}

void StartDistributionRoom::SetHeight(int height)
{
     _height = height;
}

int StartDistributionRoom::GetRoomId() const
{
     return _roomID;
}

void StartDistributionRoom::SetRoomId(int roomId)
{
     _roomID = roomId;
}

int StartDistributionRoom::GetRouteId() const
{
     return _routeID;
}

void StartDistributionRoom::SetRouteId(int routeId)
{
     _routeID = routeId;
}

int StartDistributionRoom::GetRouterId() const
{
     return _routerID;
}

void StartDistributionRoom::SetRouterId(int routerId)
{
     _routerID = routerId;
}

void StartDistributionRoom::SetAgentsNumber(int N)
{
     _nPeds = N;
}


/************************************************************
 StartDistributionSubRoom
 ************************************************************/
StartDistributionSubroom::StartDistributionSubroom(unsigned int seed) : StartDistributionRoom(seed)
{
     _subroomID = -1;
}


StartDistributionSubroom::~StartDistributionSubroom()
{
}

int StartDistributionSubroom::GetSubroomID() const
{
     return _subroomID;
}

// Setter-Funktionen

void StartDistributionSubroom::SetSubroomID(int i)
{
     _subroomID = i;
}


/************************************************************
 PedDistributor
 ************************************************************/

PedDistributor::PedDistributor(const string& fileName, const std::map<int, AgentsParameters*>& agentPars, unsigned int seed)
{
     _start_dis = vector<StartDistributionRoom* > ();
     _start_dis_sub = vector<StartDistributionSubroom* > ();
     InitDistributor(fileName, agentPars, seed);
}


PedDistributor::~PedDistributor()
{

     for (unsigned int i = 0; i < _start_dis.size(); i++) {
          delete _start_dis[i];
     }
     for (unsigned int i = 0; i < _start_dis_sub.size(); i++) {
          delete _start_dis_sub[i];
     }
     _start_dis_sub.clear();
     _start_dis.clear();
     //empty the parameters maps
}

bool PedDistributor::InitDistributor(const string& fileName, const std::map<int, AgentsParameters*>& agentPars, unsigned int seed)
{
     Log->Write("INFO: \tLoading and parsing the persons attributes");

     TiXmlDocument doc(fileName);

     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \t could not parse the project file");
          return false;
     }


     TiXmlNode* xRootNode = doc.RootElement()->FirstChild("agents");
     if( ! xRootNode ) {
          Log->Write("ERROR:\tcould not load persons attributes");
          return false;
     }


     TiXmlNode* xDist=xRootNode->FirstChild("agents_distribution");
     for(TiXmlElement* e = xDist->FirstChildElement("group"); e;
               e = e->NextSiblingElement("group")) {

          int room_id = xmltoi(e->Attribute("room_id"));
          int group_id = xmltoi(e->Attribute("group_id"));
          int subroom_id = xmltoi(e->Attribute("subroom_id"),-1);
          int number = xmltoi(e->Attribute("number"),0);
          int agent_para_id= xmltoi(e->Attribute("agent_parameter_id"),-1);

          int goal_id = xmltoi(e->Attribute("goal_id"), FINAL_DEST_OUT);
          int router_id = xmltoi(e->Attribute("router_id"), -1);
          int route_id = xmltoi(e->Attribute("route_id"), -1);
          int age = xmltoi(e->Attribute("age"), -1);
          string gender = xmltoa(e->Attribute("gender"), "male");
          double height = xmltof(e->Attribute("height"), -1);
          double patience=  xmltof(e->Attribute("patience"), 5);
          double premovement_mean= xmltof(e->Attribute("pre_movement_mean"), 0);
          double premovement_sigma= xmltof(e->Attribute("pre_movement_sigma"), 0);

          double x_min=xmltof(e->Attribute("x_min"), -FLT_MAX);
          double x_max=xmltof(e->Attribute("x_max"), FLT_MAX);
          double y_min=xmltof(e->Attribute("y_min"), -FLT_MAX);
          double y_max=xmltof(e->Attribute("y_max"), FLT_MAX);
          double bounds [4] = {x_min,x_max,y_min,y_max};

          //sanity check
          if((x_max<x_min) || (y_max<y_min) ){
              Log->Write("ERROR:\tinvalid bounds [%0.2f,%0.2f,%0.2f,%0.2f] of the group [%d]. Max and Min values mismatched?",group_id,x_min,x_max,y_min,y_max);
              return false;
          }

          StartDistributionRoom* dis=NULL;

          if(subroom_id==-1) {
               dis = new StartDistributionRoom(seed);
               _start_dis.push_back(dis);
          } else {
               dis = new StartDistributionSubroom(seed);
               dynamic_cast<StartDistributionSubroom*>(dis)->SetSubroomID(subroom_id);
               _start_dis_sub.push_back(dynamic_cast<StartDistributionSubroom*>(dis));
          }

          dis->SetRoomID(room_id);
          dis->Setbounds(bounds);
          dis->SetAgentsNumber(number);
          dis->SetAge(age);
          dis->SetGender(gender);
          dis->SetGoalId(goal_id);
          dis->SetRouteId(route_id);
          dis->SetRouterId(router_id);
          dis->SetHeight(height);
          dis->SetPatience(patience);
          dis->InitPremovementTime(premovement_mean,premovement_sigma);

          if(agentPars.count(agent_para_id)==0)
          {
              Log->Write("ERROR:\t Please specify which set of agents parameters (agent_parameter_id) to use for the group [%d]!",group_id);
              Log->Write("ERROR:\t Default values are not implemented yet");
              return false;
          }
          dis->SetGroupParameters(agentPars.at(agent_para_id));

          if(e->Attribute("start_x") && e->Attribute("start_y")) {
               double startX = xmltof(e->Attribute("start_x"),NAN);
               double startY = xmltof(e->Attribute("start_y"),NAN);
               Log->Write("INFO:\tstart_x = %f, start_y = %f\n", startX, startY);
               dis->SetStartPosition(startX,startY,0.0);
          }
     }

     //Parse the sources
     TiXmlNode* xSources=xRootNode->FirstChild("agents_sources");
     if(xSources)
          for(TiXmlElement* e = xSources->FirstChildElement("source"); e;
                    e = e->NextSiblingElement("source")) {
               Log->Write("INFO:\tSource with id %s will not be parsed !",e->Attribute("id"));

          }

     Log->Write("INFO: \t...Done");
     return true;
}

bool PedDistributor::Distribute(Building* building) const
{
     Log->Write("INFO: \tInit Distribute");
     int nPeds = 0;

     // store the position in a map since we are not computing for all rooms/subrooms.
     std::map <int, std::map <int, vector <Point> > > allFreePos;

     //collect the available positions for that subroom
     for(const auto& dist: _start_dis_sub)
     {
          int roomID = dist->GetRoomId();
          Room* r = building->GetRoom(roomID);
          if(!r) return false;

          int subroomID = dist->GetSubroomID();
          SubRoom* sr = r->GetSubRoom(subroomID);
          if(!sr) return false;

          auto&  allFreePosRoom=allFreePos[roomID];
          // the positions were already computed
          if(allFreePosRoom.count(subroomID)>0)
               continue;

          allFreePosRoom[subroomID]=PedDistributor::PossiblePositions(*sr);
     }

     //collect the available positions for that room
     for(const auto& dist: _start_dis)
     {
          int roomID = dist->GetRoomId();
          Room* r = building->GetRoom(roomID);
          if(!r) return false;

          //compute all subrooms since no specific one is given
          for (const auto& it_sr: r->GetAllSubRooms())
          {
               int subroomID=it_sr.second->GetSubRoomID();
               auto&  allFreePosRoom=allFreePos[roomID];
               // the positions were already computed
               if(allFreePosRoom.count(subroomID)>0)
                    continue;

               allFreePosRoom[subroomID]=PedDistributor::PossiblePositions(*it_sr.second);
          }
     }


     // now proceed to the distribution
     int pid = 1; // the pedID is being increased throughout...

     for(const auto& dist: _start_dis_sub)
     {
          int room_id = dist->GetRoomId();
          Room* r = building->GetRoom(room_id);
          if(!r) continue;
          int roomID = r->GetID();
          int subroomID = dist->GetSubroomID();
          int N = dist->GetAgentsNumber();
          SubRoom* sr = r->GetSubRoom(subroomID);
          if(!sr) continue;

          if (N < 0) {
               Log->Write("ERROR: \t negative  number of pedestrians!");
               return false;
          }

          auto& allpos = allFreePos[roomID][subroomID];

          int max_pos = allpos.size();
          if (max_pos < N)
          {
               Log->Write("ERROR: \tCannot distribute %d agents in Room %d . Maximum allowed: %d\n",
                          N, roomID, allpos.size());
              return false;
          }

          // Distributing
          Log->Write("INFO: \tDistributing %d Agents in Room/Subrom [%d/%d]! Maximum allowed: %d", N, roomID, subroomID, max_pos);
          DistributeInSubRoom(sr, N, allpos, &pid,dist,building);
          Log->Write("\t...Done");
          nPeds += N;
     }

     // then continue the distribution according to the rooms
     for(const auto& dist: _start_dis)
     {
          int room_id = dist->GetRoomId();
          Room* r = building->GetRoom(room_id);
          if(!r) continue;
          int N = dist->GetAgentsNumber();
          if (N < 0) {
               Log->Write("ERROR: \t negative or null number of pedestrians! Ignoring");
               continue;
          }

          double sum_area = 0;
          int max_pos = 0;
          double ppm; // pedestrians per square meter
          int ges_anz = 0;
          vector<int> max_anz = vector<int>();
          vector<int> akt_anz = vector<int>();

          auto&  allFreePosInRoom=allFreePos[room_id];
          for (int is = 0; is < r->GetNumberOfSubRooms(); is++) {
               SubRoom* sr = r->GetSubRoom(is);
               double area = sr->GetArea();
               sum_area += area;
               int anz = allFreePosInRoom[is].size();
               max_anz.push_back(anz);
               max_pos += anz;
          }
          if (max_pos < N) {
               Log->Write("ERROR: \t Distribution of %d pedestrians in Room %d not possible! Maximum allowed: %d\n",
                         N, r->GetID(), max_pos);
               return false;
          }
          ppm = N / sum_area;
          // Anzahl der Personen pro SubRoom bestimmen
          for (int is = 0; is < r->GetNumberOfSubRooms(); is++) {
               SubRoom* sr = r->GetSubRoom(is);
               int anz = sr->GetArea() * ppm + 0.5; // wird absichtlich gerundet
               while (anz > max_anz[is]) {
                    anz--;
               }
               akt_anz.push_back(anz);
               ges_anz += anz;
          }
          // Falls N noch nicht ganz erreicht, von vorne jeweils eine Person dazu
          int j = 0;
          while (ges_anz < N) {
               if (akt_anz[j] < max_anz[j]) {
                    akt_anz[j] = akt_anz[j] + 1;
                    ges_anz++;
               }
               j = (j + 1) % max_anz.size();
          }
          j = 0;
          while (ges_anz > N) {
               if (akt_anz[j] > 0) {
                    akt_anz[j] = akt_anz[j] - 1;
                    ges_anz--;
               }
               j = (j + 1) % max_anz.size();
          }
          // distributing
          for (unsigned int is = 0; is < akt_anz.size(); is++) {
               SubRoom* sr = r->GetSubRoom(is);
               if (akt_anz[is] > 0)
                    DistributeInSubRoom(sr, akt_anz[is], allFreePosInRoom[is], &pid, (StartDistributionSubroom*)dist,building);
          }
          nPeds += N;
     }

     return nPeds;
}


vector<Point> PedDistributor::PositionsOnFixX(double min_x, double max_x, double min_y, double max_y,
          const SubRoom& r, double bufx, double bufy, double dy)
{
     vector<Point> positions;
     double x = (max_x + min_x)*0.5;
     double y = min_y;

     while (y < max_y) {
          Point pos = Point(x, y);
          // Abstand zu allen Wänden prüfen
          int k;
          for (k = 0; k < r.GetNumberOfWalls(); k++) {
               if (r.GetWall(k).DistTo(pos) < max(bufx, bufy) || !r.IsInSubRoom(pos)) {
                    break; // Punkt ist zu nah an einer Wand oder nicht im Raum => ungültig
               }
          }
          if (k == r.GetNumberOfWalls()) {
               //check all transitions
               bool tooNear=false;
               for(unsigned int t=0; t<r.GetAllTransitions().size(); t++) {
                    if(r.GetTransition(t)->DistTo(pos)<J_EPS_GOAL) {
                         //too close
                         tooNear=true;
                         break;
                    }
               }

               for(unsigned int c=0; c<r.GetAllCrossings().size(); c++) {
                    if(r.GetCrossing(c)->DistTo(pos)<J_EPS_GOAL) {
                         //too close
                         tooNear=true;
                         break;
                    }
               }
               if(tooNear==false) positions.push_back(pos);
          }
          y += dy;
     }
     return positions;

}

vector<Point>PedDistributor::PositionsOnFixY(double min_x, double max_x, double min_y, double max_y,
          const SubRoom& r, double bufx, double bufy, double dx)
{
     vector<Point> positions;
     double y = (max_y + min_y)*0.5;
     double x = min_x;

     while (x < max_x) {
          Point pos = Point(x, y);
          // check distance to wall
          int k;
          for (k = 0; k < r.GetNumberOfWalls(); k++) {
               if (r.GetWall(k).DistTo(pos) < max(bufx, bufy) || !r.IsInSubRoom(pos)) {
                    break; // Punkt ist zu nah an einer Wand oder nicht im Raum => ungültig
               }
          }
          if (k == r.GetNumberOfWalls()) {
               //check all transitions
               bool tooNear=false;
               for(unsigned int t=0; t<r.GetAllTransitions().size(); t++) {
                    if(r.GetTransition(t)->DistTo(pos)<J_EPS_GOAL) {
                         //too close
                         tooNear=true;
                         break;
                    }
               }

               for(unsigned int c=0; c<r.GetAllCrossings().size(); c++) {
                    if(r.GetCrossing(c)->DistTo(pos)<J_EPS_GOAL) {
                         //too close
                         tooNear=true;
                         break;
                    }
               }
               if(tooNear==false) positions.push_back(pos);
          }
          x += dx;
     }
     return positions;
}

vector<Point >  PedDistributor::PossiblePositions(const SubRoom& r)
{
     double uni = 0.7; // wenn ein Raum in x oder y -Richtung schmaler ist als 0.7 wird in der Mitte verteilt
     double bufx = 0.12;
     double bufy = 0.12;

     double amin=0.18; // = GetAmin()->GetMean();
     double bmax=0.25; // = GetBmax()->GetMean();

     double dx = amin + bufx;
     double dy = bmax + bufy;

     double max_buf=max(bufx, bufy);

     vector<double>::iterator min_x, max_x, min_y, max_y;
     const vector<Point>& poly = r.GetPolygon();
     vector<Point > all_positions;
     vector<double> xs;
     vector<double> ys;

     for (int p = 0; p < (int) poly.size(); ++p) {
          xs.push_back(poly[p].GetX());
          ys.push_back(poly[p].GetY());
     }

     min_x = min_element(xs.begin(), xs.end());
     max_x = max_element(xs.begin(), xs.end());
     min_y = min_element(ys.begin(), ys.end());
     max_y = max_element(ys.begin(), ys.end());

     if (*max_y - *min_y < uni) {
          all_positions = PositionsOnFixY(*min_x, *max_x, *min_y, *max_y, r, bufx, bufy, dx);
     } else if (*max_x - *min_x < uni) {
          all_positions = PositionsOnFixX(*min_x, *max_x, *min_y, *max_y, r, bufx, bufy, dy);
     } else {
          // create the grid
          double x = (*min_x);
          while (x < *max_x)
          {
               double y = (*min_y);
               while (y < *max_y)
               {
                    y += dy;
                    Point pos = Point(x, y);
                    bool tooNear=false;

                    //skip if the position is not in the polygon
                    if(!r.IsInSubRoom(pos)) continue;

                    // check the distance to all Walls
                    for (const auto & w : r.GetAllWalls())
                    {
                         if (w.DistTo(pos) < max_buf) {
                              tooNear=true;
                              break; // too close
                         }
                    }

                    //check the distance to all transitions
                    if(tooNear==true) continue;
                    for (const auto & t : r.GetAllTransitions())
                    {
                         if(t->DistTo(pos)<max_buf) {
                              //too close
                              tooNear=true;
                              break;
                         }
                    }

                    //  and check the distance to all crossings
                    if(tooNear==true) continue;
                    for (const auto & c : r.GetAllCrossings())
                    {
                         if(c->DistTo(pos)<max_buf) {
                              //too close
                              tooNear=true;
                              break;
                         }
                    }

                    // and finally the distance to all opened obstacles
                    if(tooNear==true) continue;

                    for (const auto & obst : r.GetAllObstacles())
                    {
                         for (const auto & wall : obst->GetAllWalls())
                         {
                              if (wall.DistTo(pos) < max_buf) {
                                   tooNear=true;
                                   break; // too close
                              }
                         }

                         //only continue if...
                         if(tooNear==true) continue;

                         if((obst->GetClosed()==1) && (obst->Contains(pos)==true))
                         {
                              tooNear=true;
                              break; // too close
                         }
                    }

                    if(tooNear==false) all_positions.push_back(pos);
               }
               x += dx;
          }
     }

     std::random_shuffle(all_positions.begin(), all_positions.end());
     return all_positions;
}
/* Verteilt N Fußgänger in SubRoom r
 * Algorithms:
 *   - Lege Gitter von min_x bis max_x mit Abstand dx und entsprechend min_y bis max_y mit
 *     Abstand dy
 *   - Prüfe alle so enstandenen Gitterpunkte, ob ihr Abstand zu Wänden und Übergängen aus
 *     reicht
 *   - Wenn nicht verwerfe Punkt, wenn ja merke Punkt in Vector positions
 *   - Wähle zufällig einen Punkt aus dem Vektor aus Position der einzelnen Fußgänger
 * Parameter:
 *   - r:       SubRoom in den verteilt wird
 *   - N:       Anzahl der Fußgänger für SubRoom r
 *   - pid:     fortlaufender Index der Fußgänger (wird auch wieder zurückgegeben, für den
 *              nächsten Aufruf)
 *   - routing: wird benötigt um die Zielline der Fußgänger zu initialisieren
 * */
void PedDistributor::DistributeInSubRoom(SubRoom* r,int nAgents , vector<Point>& positions, int* pid,
          StartDistributionSubroom* para, Building* building) const
{

    //in the case a range was specified
    double distArea[4];
    para->Getbounds(distArea);
    AgentsParameters* agents_para=para->GetGroupParameters();

    // set the pedestrians
    for (int i = 0; i < nAgents; ++i) {

        Pedestrian* ped = new Pedestrian();
        // PedIndex
        ped->SetID(*pid);
        ped->SetAge(para->GetAge());
        ped->SetGender(para->GetGender());
        ped->SetHeight(para->GetHeight());
        ped->SetFinalDestination(para->GetGoalId());
        ped->SetGroup(para->GetGroupId());
        ped->SetRouter(building->GetRoutingEngine()->GetRouter(para->GetRouterId()));
        //ped->SetTrip(); // not implemented

        // a und b setzen muss vor v0 gesetzt werden,
        // da sonst v0 mit Null überschrieben wird
        JEllipse E = JEllipse();
        E.SetAv(agents_para->GetAtau());
        E.SetAmin(agents_para->GetAmin());
        E.SetBmax(agents_para->GetBmax());
        E.SetBmin(agents_para->GetBmin());
        ped->SetEllipse(E);
        ped->SetTau(agents_para->GetTau());
        ped->SetV0Norm(agents_para->GetV0(),
                  agents_para->GetV0DownStairs(),
                  agents_para->GetV0UpStairs());
        //ped->SetV(Point(0.0,0.0));

        // first default Position
        int index = -1;
        //int index = rand() % positions.size();

        //in the case a range was specified
        for (unsigned int a=0;a<positions.size();a++)
        {
            Point pos=positions[a];
            if((distArea[0]<=pos._x) &&
                    (pos._x <= distArea[1])&&
                    (distArea[2]<=pos._y) &&
                    (pos._y < distArea[3]))
            {
                index=a;
                break;
            }
        }
        if(index==-1)
        {
            Log->Write("ERROR:\t Cannot distribute pedestrians in the mentioned area [%0.2f,%0.2f,%0.2f,%0.2f]",
                    distArea[0],distArea[1],distArea[2],distArea[3]);
            Log->Write("ERROR:\t Specifying a subroom_id might help");
        }

        Point pos = positions[index];
        ped->SetPos(pos,true); //true for the initial position
        ped->SetBuilding(building);
        positions.erase(positions.begin() + index);
        ped->SetRoomID(para->GetRoomId(),"");
        ped->SetSubRoomID(r->GetSubRoomID());
        ped->SetPatienceTime(para->GetPatience());
        ped->SetPremovementTime(para->GetPremovementTime());
        const Point& start_pos = para->GetStartPosition();


        if((std::isnan(start_pos._x)==0 ) && (std::isnan(start_pos._y)==0 ) ) {
            if(r->IsInSubRoom(start_pos)==false){
                Log->Write("ERROR: \t cannot distribute pedestrian %d in Room %d at fixed position %s",
                                    *pid, para->GetRoomId(), start_pos.toString().c_str());
                Log->Write("ERROR: \t Make sure that the position is inside the geometry and belongs to the specified room / subroom");
                exit(EXIT_FAILURE);
            }

            ped->SetPos(start_pos,true); //true for the initial position
            Log->Write("INFO: \t fixed position for ped %d in Room %d %s",
                    *pid, para->GetRoomId(), start_pos.toString().c_str());
        }

        //r->AddPedestrian(ped);
        building->AddPedestrian(ped);
        (*pid)++;
    }
}


void StartDistributionRoom::SetStartPosition(double x, double y, double z)
{
     if(_nPeds!=1) {
          Log->Write("INFO:\t you cannot specify the same start position for many agents");
          Log->Write("INFO:\t Ignoring the start position");
          return;
     }
     _startX=x;
     _startY=y;
     _startZ=z;
}

Point StartDistributionRoom::GetStartPosition() const
{
     return Point(_startX, _startY);
}

double StartDistributionRoom::GetPatience() const
{
     return _patience;
}

void StartDistributionRoom::SetPatience(double patience)
{
     _patience = patience;
}

AgentsParameters* StartDistributionRoom::GetGroupParameters()
{
    return _groupParameters;
}

void StartDistributionRoom::SetGroupParameters(AgentsParameters* groupParameters)
{
    _groupParameters = groupParameters;
}

void StartDistributionRoom::Getbounds(double bounds[4])
{
    bounds[0]=_xMin;
    bounds[1]=_xMax;
    bounds[2]=_yMin;
    bounds[3]=_yMax;
}

void StartDistributionRoom::Setbounds(double bounds[4])
{
    _xMin=bounds[0];
    _xMax=bounds[1];
    _yMin=bounds[2];
    _yMax=bounds[3];
}

void StartDistributionRoom::InitPremovementTime(double mean, double stdv)
{
     _premovementTime = std::normal_distribution<double>(mean,stdv);
}

double StartDistributionRoom::GetPremovementTime()
{
     return _premovementTime(_generator);
}
