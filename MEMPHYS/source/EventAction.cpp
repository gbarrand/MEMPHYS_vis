#include "../MEMPHYS/EventAction.hh"

//Geant4
#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4EventManager.hh"
#include "G4UImanager.hh"
#include "G4TrajectoryContainer.hh"
#include "G4VVisManager.hh"
#include "G4ios.hh"
#include "globals.hh"
#include "G4ThreeVector.hh"
#include "G4TransportationManager.hh" 
#include "G4Navigator.hh" 
#include "G4SDManager.hh"
#include "G4DigiManager.hh"
#include "G4UnitsTable.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4Version.hh"

//std
#include <set>
#include <iomanip>
#include <string>
#include <vector>

//MEMPHYS
#include "../MEMPHYS/Analysis.hh"
#include "../MEMPHYS/Trajectory.hh"
#include "../MEMPHYS/RunAction.hh"
#include "../MEMPHYS/PrimaryGeneratorAction.hh"
#include "../MEMPHYS/WCHit.hh"
#include "../MEMPHYS/WCDigi.hh"
#include "../MEMPHYS/WCDigitizer.hh"
#include "../MEMPHYS/DetectorConstruction.hh"

#include <inlib/mnmx>

MEMPHYS::EventAction::EventAction(MEMPHYS::Analysis& aAnalysis,
				  MEMPHYS::RunAction& myRun, 
				  MEMPHYS::DetectorConstruction& myDetector, 
				  MEMPHYS::PrimaryGeneratorAction& myGenerator)
:fAnalysis(aAnalysis)
,fRunAction(myRun)
,fGeneratorAction(myGenerator)
,fDetectorConstructor(myDetector)
,m_verbose(1) 
{
  
  G4DigiManager* DMman = G4DigiManager::GetDMpointer(); //JEC FIXME: be a data member
  WCDigitizer* WCDM = new WCDigitizer( "WCReadout");
  DMman->AddNewModule(WCDM);
  
}//Ctor

//-------------------------------------------------------------------------------------------

void MEMPHYS::EventAction::EndOfEventAction(const G4Event* evt) {
  if(m_verbose==1) G4cout << " (JEC) EventAction::End EventAction" << G4endl;
  
  // --------------------
  //  Get Particle Table
  // --------------------

  G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();

  
  //JEC 10/11/05 see Analysis.cpp to get the Tuple variables

  // --------------------
  //  Get Event Information
  // --------------------
  //JEC FIXME: difference between event_id &  vecRecNumber
  G4int         event_id = evt->GetEventID();                      
  G4int         vecRecNumber = fGeneratorAction.GetVecRecNumber(); 
  G4int         mode     = fGeneratorAction.GetMode();             


  G4ThreeVector vtx = fGeneratorAction.GetVtx();                   
  G4int         vtxvol   = EventFindStartingVolume(vtx);    //JEC FIXME change to string saving

  if(m_verbose==1) {
  G4cout << ">>>>>>>>>>>>> New Event ";
  G4cout << " evt Id " << event_id 
	 << " evt Input Id " << vecRecNumber
	 << "\n interaction mode " << mode
	 << " start in volume " << vtxvol
	 << G4endl;
  G4cout << "Vertex (" 
	 << vtx.x()/CLHEP::cm << " , "
	 << vtx.y()/CLHEP::cm << " , "
	 << vtx.z()/CLHEP::cm << " , "
	 << ")"
	 << G4endl;
  }
  
  //JEC FIXME how to set flag? G4int flag; 
  // mustop, pstop, npar will be filled later

  // Next in the ntuple is an array of tracks.
  // We will keep count with npar

  // First two tracks are special: beam and target

  //----------------
  // The beam
  //----------------
  G4int  beampdg = fGeneratorAction.GetBeamPDG();
  int pId =  beampdg;
  int parent = 0;
  float timeStart = 0;
  
  G4ThreeVector beamdir = fGeneratorAction.GetBeamDir();
  double dx = beamdir.x();
  double dy = beamdir.y();
  double dz = beamdir.z();

  double mass = 0;
  G4double beamenergy = fGeneratorAction.GetBeamEnergy();
  double pTot = beamenergy;
  double ETot = pTot; //ETot (= pTot for neutrino)
  
  double px = pTot * dx;
  double py = pTot * dy;
  double pz = pTot * dz;
  
  int startVol = -1;
  int stopVol  = -1;

  bool dump_track = false;
  //dump_track = true;
  
  if(m_verbose==1) {
  if(dump_track)
  G4cout << "----> Tk{Beam}: " 
	 << " pId " << pId
	 << " parent " << parent
	 << " creation time " << timeStart 
	 << " Volumes " << startVol << " " << stopVol << "\n"
	 << " dx,dy,dz " << dx << " " << dy << " " << dz << "\n"
	 << " m " << mass
	 << " ETot " << ETot
	 << " pTot " << pTot
	 << " px,py,pz " << px << " " << py << " " << pz << "\n"
	 << G4endl;
  }

#ifdef APP_USE_INLIB_WROOT
  if(fAnalysis.use_file()) {
  fAnalysis.m_Event_track_tree->reset();
  fAnalysis.m_Event_hit_tree->reset();
  fAnalysis.m_Event_digit_tree->reset();
  } //use_file
#else //APP_USE_INLIB_WROOT
  // quiet warnings :
  (void)fAnalysis;
#endif

  fill_track(pId,parent,timeStart,dx,dy,dz,
             mass,pTot,ETot,px,py,pz,
             vtx.x()/CLHEP::cm,vtx.y()/CLHEP::cm,vtx.z()/CLHEP::cm,
             vtx.x()/CLHEP::cm,vtx.y()/CLHEP::cm,vtx.z()/CLHEP::cm,
             startVol,stopVol);

  //----------------
  // The target
  //----------------
  G4int  targetpdg    = fGeneratorAction.GetTargetPDG();
  pId =  targetpdg;
  parent = 0;    //none
  timeStart = 0; //creation time
  
  G4ThreeVector targetdir = fGeneratorAction.GetTargetDir();
  dx = targetdir.x();
  dy = targetdir.y();
  dz = targetdir.z();
  
  G4double targetenergy = fGeneratorAction.GetTargetEnergy();
  G4double targetpmag = 0.0, targetmass = 0.0;
  
  //JEC FIXME: keep original code for the moment
  if (targetpdg!=0) {            // protects against seg-fault
    if ( 8016 == targetpdg ) {   // JEC 25/11/05 Oxygen 16 in Nuance PID = Z*1000+A = 8016
      targetmass = targetenergy; // supposed at Rest
    } else {
      G4ParticleDefinition* pdef = particleTable->FindParticle(targetpdg);
      if(!pdef) { //G.Barrand.
        G4cout << "EventAction::EndOfEventAction : FindParticle(" << targetpdg << ") failed." << G4endl;
        return;
      }
      targetmass = pdef->GetPDGMass();
    }
    if (targetenergy > targetmass) {
      targetpmag = sqrt(targetenergy*targetenergy - targetmass*targetmass);
    } else { // protect against NaN
      targetpmag = 0.0;
    }
  }

  mass = targetmass;
  pTot =  targetpmag;
  ETot = targetenergy;
  px = pTot * dx;
  py = pTot * dy;
  pz = pTot * dz;

  startVol = -1;
  stopVol  = -1;

  if(m_verbose==1) {
  if(dump_track)
  G4cout << "----> Tk{Target}: " 
	 << " pId " << pId
	 << " parent " << parent
	 << " creation time " << timeStart 
	 << " Volumes " << startVol << " " << stopVol << "\n"
	 << " dx,dy,dz " << dx << " " << dy << " " << dz << "\n"
	 << " m " << mass
	 << " ETot " << ETot
	 << " pTot " << pTot
	 << " px,py,pz " << px << " " << py << " " << pz << "\n"
	 << G4endl;
  }
  
  fill_track(pId,parent,timeStart,dx,dy,dz,
             mass,pTot,ETot,px,py,pz,
             vtx.x()/CLHEP::cm,vtx.y()/CLHEP::cm,vtx.z()/CLHEP::cm,
             vtx.x()/CLHEP::cm,vtx.y()/CLHEP::cm,vtx.z()/CLHEP::cm,
             startVol,stopVol);

  // --------------------------
  //  Loop over Trajectories
  // --------------------------

  G4TrajectoryContainer* trajectoryContainer = evt->GetTrajectoryContainer();

  G4int n_trajectories = 0;
  if (trajectoryContainer) n_trajectories = trajectoryContainer->entries();

  std::set<int> pizeroList;
  std::set<int> muonList;
  std::set<int> antimuonList;
  std::set<int> pionList;
  std::set<int> antipionList;

  G4int trjIndex;    //JEC 14/11/05 this is the trajectory Index
  G4int parentIndex; //JEC 14/11/05 this the parent trajectory index

  //JEC 14/11/05 factorize declarations...
  G4ThreeVector mom; //px,py,pz
  G4double      mommag; //p = sqrt(px*px + ...)
  G4ThreeVector stop; //stopping point
  G4ThreeVector start;//starting point
  G4String stopVolumeName; //Volume name of the stopping point for retreive
  G4int ntrack = 2; //count tracks that are stored in Tuple (start at 2 for the Beam + Target)

  //JEC 21/11/05
  G4int leadingLeptonIndex = -1;    //Index of the most energetic lepton issued from the neutrino interaction
  G4double tmpEnergyleadingLepton = -1.0; //temproary variable to trigger the update of leadingLeptonIndex
  G4int outgoingProtonIndex= -1;    //Index of the most energetic proton issued from the neutrino interaction
  G4double tmpEnergyoutgoingProton = -1.0; //temproary variable to trigger the update of outgoingProtonIndex
  

  for (G4int i=0; i < n_trajectories; i++) {
    Trajectory* trj = (Trajectory*)((*(evt->GetTrajectoryContainer()))[i]);
    
    // Draw Charged Tracks
    //JEC FIWME: display if (trj->GetCharge() != 0.) trj->DrawTrajectory(50);
    
    // If this track has a special id remember it for later
    
    pId      =  trj->GetPDGEncoding();                                   //pId
    trjIndex =  trj->GetTrackID();
    switch ( pId ) {
    case 111: //pi0
      pizeroList.insert( trjIndex );
      break;
    case 13: //muon
      muonList.insert( trjIndex );
      break;
    case -13: //anti-mu
      antimuonList.insert( trjIndex );
      break;
    case 211: //pion
      pionList.insert( trjIndex );
      break;
    case -211: //anti-pion
      antipionList.insert( trjIndex );
      break;
    }//eosw
      
    // Process primary tracks or the secondaries from pizero or muons, pions...
    if ( ! trj->GetSaveFlag() ) continue;

    // initial point of the trajectory
    G4TrajectoryPoint* aa =   (G4TrajectoryPoint*)trj->GetPoint(0) ;   
    fRunAction.incrementEventsGenerated(); //JEC FIXME: what is the need of that?
    
    //JEC FIXME should be META info G4int         flag   = 0;    // will be set later
    
    //JEC 14/11/06 code adapeted from M.F
    parentIndex = trj->GetParentID();
    if (parentIndex == 0){
      parent = 0;
    } else if ( pizeroList.count(parentIndex) ) {
      parent = 111;
    } else if ( muonList.count(parentIndex) ) {
      parent = 13;
    } else if ( antimuonList.count(parentIndex) ) {
      parent = -13;
    } else if ( antipionList.count(parentIndex) ) {
      parent = -211;
    } else if ( pionList.count(parentIndex) ) {
      parent = 211;
    } else {  // no identified parent, but not a primary
      parent = 999;
    }//eoif
    
    timeStart =  trj->GetGlobalTime()/CLHEP::ns;
    
    mom    = trj->GetInitialMomentum();
    mommag = mom.mag();
    dx = mom.x()/mommag;
    dy = mom.y()/mommag;
    dz = mom.z()/mommag;
    
    mass = trj->GetParticleDefinition()->GetPDGMass();
    pTot = mommag;
    ETot = sqrt(mom.mag2() + mass*mass);

    //Save index of the most energetic primary lepton and proton
    if ( 0 == parent ) {
      if ( ( (13 == pId) || (-13 == pId) ) && 
	   ( ETot > tmpEnergyleadingLepton ) ) {
	//a mu+ or mu- from the neutrino interaction and with a greater energy than previous case, update
	tmpEnergyleadingLepton = ETot;
	//flag the particle Index
	leadingLeptonIndex = i; //JEC FIXME it may be 'ntrack' directly
      } else if ( ( (11 == pId) || (-11 == pId) ) && 
		  ( ETot > tmpEnergyoutgoingProton ) ) {
	tmpEnergyoutgoingProton = ETot;
	outgoingProtonIndex = i; //JEC FIXME it may be 'ntrack' directly
      }//eo update special particle index saving
    }//eo a primary particle
    
    px = mom.x();
    py = mom.y();
    pz = mom.z();
    
    start = aa->GetPosition();
    
    stop  = trj->GetStoppingPoint();
    
    startVol = EventFindStartingVolume(start);
    stopVolumeName = trj->GetStoppingVolume()->GetName();
    stopVol  = EventFindStoppingVolume(stopVolumeName);
    
    if(m_verbose==1) {
    if(dump_track)
    G4cout << "----> Tk{"<<ntrack<<"}: " 
	   << " pId " << pId
	   << " parent " << parent
	   << " creation time " << timeStart 
	   << " Volumes " << startVol << " " << stopVol << "\n"
	   << " Start Pos (" << start.x()/CLHEP::cm << "," << start.y() << "," << start.z() << ")\n"
	   << " Stop Pos (" << stop.x()/CLHEP::cm << "," << stop.y() << "," << stop.z() << ")\n"
	   << " dx,dy,dz " << dx << " " << dy << " " << dz << "\n"
	   << " m " << mass
	   << " ETot " << ETot
	   << " pTot " << pTot
	   << " px,py,pz " << px << " " << py << " " << pz << "\n"
	   << G4endl;
    }
    
    fill_track(pId,parent,timeStart,dx,dy,dz,
               mass,pTot,ETot,px,py,pz,
               start.x()/CLHEP::cm,start.y()/CLHEP::cm,start.z()/CLHEP::cm,
               stop.x()/CLHEP::cm,stop.y()/CLHEP::cm,stop.z()/CLHEP::cm,
               startVol,stopVol);
    
    ntrack++;

  }//end of loop on trajectories
  
  if(m_verbose==1) G4cout << "Final # of tracks : " << ntrack << G4endl;
  
  if (leadingLeptonIndex != -1 ) {
    //most energetic primary lepton found
    leadingLeptonIndex += 2; //add the "beam" + "target" index, JEC FIXME see comment above
  }

  if (outgoingProtonIndex !=-1 ) {
    outgoingProtonIndex += 2; //add the "beam" + "target" index, JEC FIXME see comment above
  }
  
  // --------------------
  //  Get WC Hit Collection
  // --------------------
    
  G4SDManager* SDman = G4SDManager::GetSDMpointer(); //JEC FIXME: use data member
    
  // Get Hit collection of this event
  G4HCofThisEvent* HCE = evt->GetHCofThisEvent();
  WCHitsCollection* WCHC = 0;

  G4int nHits=0;
  G4int tubeID_hit; //JEC 16/1/06
  G4int totalPE;
  
  G4float peArrivalTime;

  if (HCE) { 
    G4String name = "WCPMT";
    G4int collectionID = SDman->GetCollectionID(name);
    WCHC = (WCHitsCollection*)HCE->GetHC(collectionID);
  }
  if (!WCHC) {
    G4cout << "(JEC:EndOfEventAction): WARNING no WC hits collection found" << G4endl;
  }

  
  //JEC 14/6/06 START the Digitization should be done before manipulation of hit (ie. sorting the time!)
  // Get a pointer to the Digitizing Module Manager
  G4DigiManager* DMman = G4DigiManager::GetDMpointer(); //JEC FIXME: use data member
  
  // Get a pointer to the WC Digitizer module
  WCDigitizer* WCDM = (WCDigitizer*)DMman->FindDigitizerModule("WCReadout");
  if (!WCDM) {
    G4cout << "(JEC:EndOfEventAction): WARNING no WC Digitizer found" << G4endl;
  }

  if (WCDM && WCHC) {

    //JEC 14/6/06 START   
    // Figure out what size PMTs we are using in the WC detector.
    G4float PMTSize = fDetectorConstructor.GetPMTSize();
    WCDM->SetPMTSize(PMTSize);
    // Digitize the hits
    WCDM->Digitize();
    //JEC 14/6/06 END

    nHits = WCHC->entries();
 
    //JEC FIWME save the time information also later
    for (G4int i=0; i<nHits  ;i++) {
      
      (*WCHC)[i]->UpdateColour();
      
      tubeID_hit = (*WCHC)[i]->GetTubeID(); //JEC 16/1/06
      totalPE = (*WCHC)[i]->GetTotalPe();

#ifdef APP_USE_INLIB_WROOT
      if(fAnalysis.use_file()) fAnalysis.m_Event_hit_pe_vec.clear();
#endif      
      for (G4int j=0; j<inlib::mn<G4int>(100,totalPE) ; j++) {                  //JEC: limit the number of "impacts"
	peArrivalTime = (*WCHC)[i]->GetTime(j); 
#ifdef APP_USE_INLIB_WROOT
        if(fAnalysis.use_file()) fAnalysis.m_Event_hit_pe_vec.push_back(peArrivalTime);
#endif	
        fill_hit_time(peArrivalTime); //JEC 5/4/06 fill the Hit time tuple
      }
      
      fill_hit(tubeID_hit,totalPE);
    }
  }//Hit container
  

  // --------------------
  //  Get Digitized Hit Collection
  // --------------------

  //JEC 14/6/06 this part should be done before storing the Hits as the Digitization sort the times for instance
//   // Get a pointer to the Digitizing Module Manager
//   G4DigiManager* DMman = G4DigiManager::GetDMpointer(); //JEC FIXME: use data member

//   // Get a pointer to the WC Digitizer module
//   WCDigitizer* WCDM =
//     (WCDigitizer*)DMman->FindDigitizerModule("WCReadout");
//   if (!WCDM) {
//     G4cout << "(JEC:EndOfEventAction): FATAL no WC Digitizer found" << G4endl;
//     exit(0);
//   }

//   // Figure out what size PMTs we are using in the WC detector.
//   G4float PMTSize = fDetectorConstructor.GetPMTSize();
//   WCDM->SetPMTSize(PMTSize);

//   // Digitize the hits
//   WCDM->Digitize();
  
  // Get the digitized collection for the WC
  G4int WCDCID = DMman->GetDigiCollectionID("WCDigitizedCollection");
  WCDigitsCollection * WCDC = (WCDigitsCollection*)DMman->GetDigiCollection(WCDCID);
    
  G4int nDigits = 0;
  G4double sumPE = 0;
  G4int tubeID;
  G4double tubePhotoElectrons;
  G4double tubeTime;
  if(WCDC) {
    nDigits = WCDC->entries();
    for (G4int i=0; i < nDigits; i++) {
      tubeID             = (*WCDC)[i]->GetTubeID();
      
      tubePhotoElectrons = (*WCDC)[i]->GetPe();
      sumPE += tubePhotoElectrons;

      tubeTime           = (*WCDC)[i]->GetTime();
      
      fill_digit(tubeID,tubePhotoElectrons,tubeTime);
      //	(*WCDC)[i]->Print();
    }//loop on digits
  } else {
    if(m_verbose==1) G4cout << "(JEC) EventAction: No Digits for Event: "  << event_id << G4endl;
  }//digits collection

  if(m_verbose==1) {  
  G4cout << "----> Event : " 
	 << " nHits " << nHits
	 << " nDigits " << nDigits
	 << " ntrack " << ntrack
	 << " sumPE " << sumPE
	 << G4endl;
  }
  
  fill_event(event_id,vecRecNumber,mode,vtxvol,
             vtx.x()/CLHEP::cm,vtx.y()/CLHEP::cm,vtx.z()/CLHEP::cm,ntrack,
             leadingLeptonIndex,outgoingProtonIndex,
             nHits,nDigits,sumPE);
    
}//EndOfEventAction

//--------------------------------------------------------------------------------------------------

G4int MEMPHYS::EventAction::EventFindStartingVolume(G4ThreeVector vtx) {
  // Get volume of starting point (see GEANT4 FAQ)

  G4int vtxvol = -1;

  G4Navigator* tmpNavigator = 
    G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();

  G4VPhysicalVolume* tmpVolume = tmpNavigator->LocateGlobalPointAndSetup(vtx);
  G4String       vtxVolumeName = tmpVolume->GetName();


  //JEC 15/11/05 FIXME: code to be clean and keep only WC detector elements
  if(m_verbose==1) {
  G4cout << "(EventFindStartingVolume): vtxVolumeName: <"
	 << vtxVolumeName
	 << ">" << G4endl;
  }
  
  if (vtxVolumeName.contains("MRD") ){
    vtxvol = 30;
  } else if ( vtxVolumeName.contains("FGD") ){
    vtxvol = 20;
  } else if ( vtxVolumeName.contains("lAr") ){
    vtxvol = 20;
  } else if ( vtxVolumeName == "outerTube" ||
	    vtxVolumeName == "innerTube" ||
	    vtxVolumeName == "rearEndCap"|| 
	    vtxVolumeName == "frontEndCap" ){
      vtxvol = 10;
  } else if ( vtxVolumeName.contains("WC") ){
    if (vtxVolumeName == "WCBarrel")
      vtxvol = 10;
    else if (vtxVolumeName == "WCBox")
      vtxvol = -2;
    else 
      {
	if (vtxVolumeName.contains("PMT") ||
	    vtxVolumeName.contains("Cap") ||
	    vtxVolumeName.contains("Cell"))
	  vtxvol = 11;
	else if (vtxVolumeName.contains("OD"))
	  vtxvol = 12;
	else
	  {
	    G4cout << vtxVolumeName << " unkown vtxVolumeName " << G4endl;
	    vtxvol = -3;
	  }
      }
  } else if ( vtxVolumeName == "expHall" ){
    vtxvol = 0;
  } else if ( vtxVolumeName == "catcher" ){
    vtxvol = 40;
  } else if (vtxVolumeName.contains("Rock") ){
    vtxvol = 50;
  }
  
  return vtxvol;
}//EventFindStartingVolume

//--------------------------------------------------------------------------------------------------

G4int MEMPHYS::EventAction::EventFindStoppingVolume(G4String stopVolumeName) {
  G4int stopvol = -1;

  //JEC 15/11/05 FIXME: code to be clean and keep only WC detector elements
  if ( stopVolumeName.contains("MRD") )
    {
      //JEC 18/11/05 not used      fRunAction.incrementMRDHits();
      stopvol = 30;
    }	
  else if ( stopVolumeName.contains("FGD") )
    {
      //JEC 18/11/05 not used fRunAction.incrementFGDHits();
      stopvol = 20;  }	
  else if (stopVolumeName.contains("lAr"))
    {
      //JEC 18/11/05 not used fRunAction.incrementlArDHits();
      stopvol = 20;
    }
  else if ( stopVolumeName == "outerTube" ||
	    stopVolumeName == "innerTube" ||
	    stopVolumeName == "rearEndCap"|| 
	    stopVolumeName == "frontEndCap" )
    {
      fRunAction.incrementWaterTubeHits();
      stopvol = 10;
    }	
  else if ( stopVolumeName.contains("WC") )
    {
      if (stopVolumeName == "WCBarrel")
	stopvol = 10;
      else if (stopVolumeName == "WCBox")
	stopvol = 0; // same as expHall
      else 
	{
	  if (stopVolumeName.contains("PMT") ||
	      stopVolumeName.contains("Cap") ||
	      stopVolumeName.contains("Cell"))
	    stopvol = 11;
	  else if (stopVolumeName.contains("OD"))
	    stopvol = 12;
	  else
	    {
	      G4cout << stopVolumeName << " unknown stopVolumeName " << G4endl;
	      stopvol = -3;
	    }
	}
      fRunAction.incrementWaterTubeHits();
    }	
  else if ( stopVolumeName == "expHall" )
    {
      stopvol = 0;
    }
  else if ( stopVolumeName == "catcher" )
    {
      fRunAction.incrementCatcherHits();
      stopvol = 40;
    }
  else if (stopVolumeName.contains("Rock") ){
    stopvol = 50;
    fRunAction.incrementRockHits();
  }
  
  return stopvol;
}//EventFindStoppingVolume



void MEMPHYS::EventAction::fill_track(int pId,int parent,float timeStart,
		                      double dx,double dy,double dz,
                                      double mass,double pTot, double ETot,double px,double py,double pz,
                                      double startPos_x,double startPos_y,double startPos_z,
                                      double stopPos_x,double stopPos_y,double stopPos_z,
             	                      int startVol,int stopVol) {
#ifdef APP_USE_INLIB_WROOT
  if(fAnalysis.use_file()) {
  fAnalysis.m_Event_track_pId = pId;
  fAnalysis.m_Event_track_parent = parent;
  fAnalysis.m_Event_track_timeStart = timeStart;

  fAnalysis.m_Event_track_direction_vec.clear();
  fAnalysis.m_Event_track_direction_vec.push_back(dx);
  fAnalysis.m_Event_track_direction_vec.push_back(dy);
  fAnalysis.m_Event_track_direction_vec.push_back(dz);
  
  fAnalysis.m_Event_track_mass = mass;
  fAnalysis.m_Event_track_pTot = pTot;
  fAnalysis.m_Event_track_ETot = ETot;

  fAnalysis.m_Event_track_momentum_vec.clear();
  fAnalysis.m_Event_track_momentum_vec.push_back(px);
  fAnalysis.m_Event_track_momentum_vec.push_back(py);
  fAnalysis.m_Event_track_momentum_vec.push_back(pz);

  fAnalysis.m_Event_track_startPos_vec.clear();
  fAnalysis.m_Event_track_startPos_vec.push_back(startPos_x);
  fAnalysis.m_Event_track_startPos_vec.push_back(startPos_y);
  fAnalysis.m_Event_track_startPos_vec.push_back(startPos_z);

  fAnalysis.m_Event_track_stopPos_vec.clear();
  fAnalysis.m_Event_track_stopPos_vec.push_back(stopPos_x);
  fAnalysis.m_Event_track_stopPos_vec.push_back(stopPos_y);
  fAnalysis.m_Event_track_stopPos_vec.push_back(stopPos_z);

  fAnalysis.m_Event_track_startVol = startVol;
  fAnalysis.m_Event_track_stopVol = stopVol;
   
 {inlib::uint32 nbytes;
  if(!fAnalysis.m_Event_track_tree->fill(nbytes)) {
    std::cout << "fAnalysis.m_Event_track_tree fill failed." << std::endl;
  }}

  } //use_file
#else
  // quiet warnings :
  (void)pId;(void)parent;(void)timeStart;
  (void)dx;(void)dy;(void)dz;
  (void)mass;(void)pTot;(void)ETot;(void)px;(void)py;(void)pz;
  (void)startPos_x;(void)startPos_y;(void)startPos_z;
  (void)stopPos_x;(void)stopPos_y;(void)stopPos_z;
  (void)startVol;(void)stopVol;
#endif 
}
 
void MEMPHYS::EventAction::fill_hit(int tubeID_hit,int totalPE) {
#ifdef APP_USE_INLIB_WROOT
  if(fAnalysis.use_file()) {
  fAnalysis.m_Event_hit_tubeId = tubeID_hit;
  fAnalysis.m_Event_hit_totalPE = totalPE;
  //fAnalysis.m_Event_hit_pe_vec = a_times;  
 {inlib::uint32 nbytes;
  if(!fAnalysis.m_Event_hit_tree->fill(nbytes)) {
    std::cout << "fAnalysis.m_Event_hit_tree fill failed." << std::endl;
  }}
  } //use_file
#else  
  // quiet warnings :
  (void)tubeID_hit;(void)totalPE;
#endif   
}

void MEMPHYS::EventAction::fill_digit(int tubeID,double tubePhotoElectrons,double tubeTime) {
#ifdef APP_USE_INLIB_WROOT
  if(fAnalysis.use_file()) {
  fAnalysis.m_Event_digit_tubeId = tubeID;
  fAnalysis.m_Event_digit_pe = tubePhotoElectrons;
  fAnalysis.m_Event_digit_time = tubeTime;
  
 {inlib::uint32 nbytes;
  if(!fAnalysis.m_Event_digit_tree->fill(nbytes)) {
    std::cout << "fAnalysis.m_Event_digit_tree fill failed." << std::endl;
  }}
  } //use_file
#else  
  // quiet warnings :
  (void)tubeID;(void)tubePhotoElectrons;(void)tubeTime;
#endif   
}				      

void MEMPHYS::EventAction::fill_event(int event_id,int vecRecNumber,int mode,int vtxvol,
				      double vtx_x,double vtx_y,double vtx_z,int ntrack,
                                      int leadingLeptonIndex,int outgoingProtonIndex,
                                      int nHits,int nDigits,double sumPE) {
#ifdef APP_USE_INLIB_WROOT
  if(fAnalysis.use_file()) {
  fAnalysis.m_Event_eventId = event_id;
  fAnalysis.m_Event_inputEvtId = vecRecNumber;
  fAnalysis.m_Event_interMode = mode;
  fAnalysis.m_Event_vtxVol = vtxvol;

  fAnalysis.m_Event_vtxPos_vec.clear();
  fAnalysis.m_Event_vtxPos_vec.push_back(vtx_x);
  fAnalysis.m_Event_vtxPos_vec.push_back(vtx_y);
  fAnalysis.m_Event_vtxPos_vec.push_back(vtx_z);
  
  fAnalysis.m_Event_nPart = ntrack;
  fAnalysis.m_Event_leptonIndex = leadingLeptonIndex;
  fAnalysis.m_Event_protonIndex = outgoingProtonIndex;
  
  fAnalysis.m_Event_nHits = nHits;
  fAnalysis.m_Event_nDigits = nDigits;
  fAnalysis.m_Event_sumPE = sumPE;
  
 {inlib::uint32 nbytes;
  if(!fAnalysis.m_Event_tree->fill(nbytes)) {
    std::cout << "m_Event_tree fill failed." << std::endl;
  }}

  } //use_file
#else
  // quiet warnings :
  (void)event_id;(void)vecRecNumber;(void)mode;(void)vtxvol;
  (void)vtx_x;(void)vtx_y;(void)vtx_z;(void)ntrack;
  (void)leadingLeptonIndex;(void)outgoingProtonIndex;
  (void)nHits;(void)nDigits;(void)sumPE;
#endif
}    

