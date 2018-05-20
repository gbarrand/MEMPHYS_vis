//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// $Id: G4VITRestDiscreteProcess.hh 100802 2016-11-02 14:55:27Z gcosmo $
//
/// \brief Identical to G4VRestDiscreteProcess with dependency from G4VITProcess
//
// WARNING : This class is released as a prototype.
// It might strongly evolve or even disapear in the next releases.
//
// Author: Mathieu Karamitros

// The code is developed in the framework of the ESA AO7146
//
// We would be very happy hearing from you, send us your feedback! :)
//
// In order for Geant4-DNA to be maintained and still open-source,
// article citations are crucial. 
// If you use Geant4-DNA chemistry and you publish papers about your software, 
// in addition to the general paper on Geant4-DNA:
//
// Int. J. Model. Simul. Sci. Comput. 1 (2010) 157–178
//
// we would be very happy if you could please also cite the following
// reference papers on chemistry:
//
// J. Comput. Phys. 274 (2014) 841-882
// Prog. Nucl. Sci. Tec. 2 (2011) 503-508 

#ifndef G4VITRESTDISCRETEPROCESS_H
#define G4VITRESTDISCRETEPROCESS_H

#include <CLHEP/Units/SystemOfUnits.h>

#include "G4VITProcess.hh"

/**
 * Identical to G4VRestDiscreteProcess with dependency from G4VITProcess
 */

class G4VITRestDiscreteProcess : public G4VITProcess
{
  //  Abstract class which defines the public behavior of
  //  rest + discrete physics interactions.
public:

  G4VITRestDiscreteProcess(const G4String&, G4ProcessType aType = fNotDefined);
  G4VITRestDiscreteProcess(const G4VITRestDiscreteProcess &);

  virtual ~G4VITRestDiscreteProcess();

public:
  // with description
  virtual G4double
  PostStepGetPhysicalInteractionLength(const G4Track& track,
                                       G4double previousStepSize,
                                       G4ForceCondition* condition);

  virtual G4VParticleChange* PostStepDoIt(const G4Track&, const G4Step&);

  virtual G4double AtRestGetPhysicalInteractionLength(const G4Track&,
                                                      G4ForceCondition*);

  virtual G4VParticleChange* AtRestDoIt(const G4Track&, const G4Step&);

  //  no operation in  AlongStepDoIt
  virtual G4double AlongStepGetPhysicalInteractionLength(const G4Track&,
                                                         G4double,
                                                         G4double,
                                                         G4double&,
                                                         G4GPILSelection*)
  {
    return -1.0;
  }

  //  no operation in  AlongStepDoIt
  virtual G4VParticleChange* AlongStepDoIt(const G4Track&, const G4Step&)
  {
    return 0;
  }

protected:
  // with description
  virtual G4double GetMeanFreePath(const G4Track& aTrack,
                                   G4double previousStepSize,
                                   G4ForceCondition* condition)=0;
  //  Calculates from the macroscopic cross section a mean
  //  free path, the value is returned in units of distance.

  virtual G4double GetMeanLifeTime(const G4Track& aTrack,
                                   G4ForceCondition* condition)=0;
  //  Calculates the mean life-time (i.e. for decays) of the
  //  particle at rest due to the occurence of the given process,
  //  or converts the probability of interaction (i.e. for
  //  annihilation) into the life-time of the particle for the
  //  occurence of the given process.

protected:
  // hide default constructor and assignment operator as private
  G4VITRestDiscreteProcess();
  G4VITRestDiscreteProcess & operator=(const G4VITRestDiscreteProcess &right);

};

// -----------------------------------------
//  inlined function members implementation
// -----------------------------------------
inline G4double G4VITRestDiscreteProcess::PostStepGetPhysicalInteractionLength(const G4Track& track,
                                                                               G4double previousStepSize,
                                                                               G4ForceCondition* condition)
{
  if((previousStepSize < 0.0) || (fpState->theNumberOfInteractionLengthLeft
      <= 0.0))
  {
    // beggining of tracking (or just after DoIt of this process)
    ResetNumberOfInteractionLengthLeft();
  }
  else if(previousStepSize > 0.0)
  {
    // subtract NumberOfInteractionLengthLeft
    SubtractNumberOfInteractionLengthLeft(previousStepSize);
  }
  else
  {
    // zero step
    //  DO NOTHING
  }

  // condition is set to "Not Forced"
  *condition = NotForced;

  // get mean free path
  fpState->currentInteractionLength = GetMeanFreePath(track,
                                                      previousStepSize,
                                                      condition);

  G4double value;
  if(fpState->currentInteractionLength < DBL_MAX)
  {
    value = fpState->theNumberOfInteractionLengthLeft * (fpState->currentInteractionLength);
  }
  else
  {
    value = DBL_MAX;
  }
#ifdef G4VERBOSE
  if(verboseLevel > 1)
  {
    G4cout << "G4VITRestDiscreteProcess::PostStepGetPhysicalInteractionLength ";
    G4cout << "[ " << GetProcessName() << "]" << G4endl;
    track.GetDynamicParticle()->DumpInfo();
    G4cout << " in Material  " << track.GetMaterial()->GetName() << G4endl;
    G4cout << "InteractionLength= " << value / CLHEP::cm << "[cm] " << G4endl;
  }
#endif
  return value;
}

inline G4VParticleChange* G4VITRestDiscreteProcess::PostStepDoIt(const G4Track&,
                                                                 const G4Step&)
{
//  reset NumberOfInteractionLengthLeft
  ClearNumberOfInteractionLengthLeft();

  return pParticleChange;
}

inline G4double G4VITRestDiscreteProcess::AtRestGetPhysicalInteractionLength(const G4Track& track,
                                                                             G4ForceCondition* condition)
{
  // beggining of tracking
  ResetNumberOfInteractionLengthLeft();

  // condition is set to "Not Forced"
  *condition = NotForced;

  // get mean life time
  fpState->currentInteractionLength = GetMeanLifeTime(track, condition);

#ifdef G4VERBOSE
  if((fpState->currentInteractionLength < 0.0) || (verboseLevel > 2))
  {
    G4cout << "G4VITRestDiscreteProcess::AtRestGetPhysicalInteractionLength ";
    G4cout << "[ " << GetProcessName() << "]" << G4endl;
    track.GetDynamicParticle()->DumpInfo();
    G4cout << " in Material  " << track.GetMaterial()->GetName() << G4endl;
    G4cout << "MeanLifeTime = " << fpState->currentInteractionLength / CLHEP::ns
           << "[ns]" << G4endl;
  }
#endif

  return fpState->theNumberOfInteractionLengthLeft
      * (fpState->currentInteractionLength);
}

inline G4VParticleChange* G4VITRestDiscreteProcess::AtRestDoIt(const G4Track&,
                                                               const G4Step&)
{
//  clear NumberOfInteractionLengthLeft
  ClearNumberOfInteractionLengthLeft();

  return pParticleChange;
}

#endif

