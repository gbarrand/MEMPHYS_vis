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
// INCL++ intra-nuclear cascade model
// Alain Boudard, CEA-Saclay, France
// Joseph Cugnon, University of Liege, Belgium
// Jean-Christophe David, CEA-Saclay, France
// Pekka Kaitaniemi, CEA-Saclay, France, and Helsinki Institute of Physics, Finland
// Sylvie Leray, CEA-Saclay, France
// Davide Mancusi, CEA-Saclay, France
//
#define INCLXX_IN_GEANT4_MODE 1

#include "globals.hh"

/*
 * G4INCLBinaryCollisionAvatar.cc
 *
 *  \date Jun 5, 2009
 * \author Pekka Kaitaniemi
 */

#include "G4INCLBinaryCollisionAvatar.hh"
#include "G4INCLElasticChannel.hh"
#include "G4INCLRecombinationChannel.hh"
#include "G4INCLDeltaProductionChannel.hh"
#include "G4INCLNNToMultiPionsChannel.hh"
#include "G4INCLNNToNNEtaChannel.hh"
#include "G4INCLNDeltaEtaProductionChannel.hh"
#include "G4INCLNNEtaToMultiPionsChannel.hh"
#include "G4INCLNNToNNOmegaChannel.hh"
#include "G4INCLNDeltaOmegaProductionChannel.hh"
#include "G4INCLNNOmegaToMultiPionsChannel.hh"
#include "G4INCLCrossSections.hh"
#include "G4INCLKinematicsUtils.hh"
#include "G4INCLRandom.hh"
#include "G4INCLParticleTable.hh"
#include "G4INCLPauliBlocking.hh"
#include "G4INCLPiNElasticChannel.hh"
#include "G4INCLPiNToDeltaChannel.hh"
#include "G4INCLPiNToMultiPionsChannel.hh"
#include "G4INCLPiNToEtaChannel.hh"
#include "G4INCLPiNToOmegaChannel.hh"
#include "G4INCLEtaNElasticChannel.hh"
#include "G4INCLEtaNToPiNChannel.hh"
#include "G4INCLEtaNToPiPiNChannel.hh"
#include "G4INCLOmegaNElasticChannel.hh"
#include "G4INCLOmegaNToPiNChannel.hh"
#include "G4INCLOmegaNToPiPiNChannel.hh"
#include "G4INCLStore.hh"
#include "G4INCLBook.hh"
#include "G4INCLLogger.hh"
#include <string>
#include <sstream>
// #include <cassert>

namespace G4INCL {

  // WARNING: if you update the default cutNN value, make sure you update the
  // cutNNSquared variable, too.
  G4ThreadLocal G4double BinaryCollisionAvatar::cutNN = 1910.0;
  G4ThreadLocal G4double BinaryCollisionAvatar::cutNNSquared = 3648100.0; // 1910.0 * 1910.0

  BinaryCollisionAvatar::BinaryCollisionAvatar(G4double time, G4double crossSection,
      G4INCL::Nucleus *n, G4INCL::Particle *p1, G4INCL::Particle *p2)
    : InteractionAvatar(time, n, p1, p2), theCrossSection(crossSection),
    isParticle1Spectator(false),
    isParticle2Spectator(false),
    isElastic(false)
  {
    setType(CollisionAvatarType);
  }

  BinaryCollisionAvatar::~BinaryCollisionAvatar() {
  }

  G4INCL::IChannel* BinaryCollisionAvatar::getChannel() {
    // We already check cutNN at avatar creation time, but we have to check it
    // again here. For composite projectiles, we might have created independent
    // avatars with no cutNN before any collision took place.
    if(particle1->isNucleon()
        && particle2->isNucleon()
        && theNucleus->getStore()->getBook().getAcceptedCollisions()!=0) {
      const G4double energyCM2 = KinematicsUtils::squareTotalEnergyInCM(particle1, particle2);
      // Below a certain cut value we don't do anything:
      if(energyCM2 < cutNNSquared) {
        INCL_DEBUG("CM energy = sqrt(" << energyCM2 << ") MeV < std::sqrt(" << cutNNSquared
            << ") MeV = cutNN" << "; returning a NULL channel" << '\n');
        InteractionAvatar::restoreParticles();
        return NULL;
      }
    }

    /** Check again the distance of approach. In order for the avatar to be
     * realised, we have to perform a check in the CM system. We define a
     * distance four-vector as
     * \f[ (0, \Delta\vec{x}), \f]
     * where \f$\Delta\vec{x}\f$ is the distance vector of the particles at
     * their minimum distance of approach (i.e. at the avatar time). By
     * boosting this four-vector to the CM frame of the two particles and we
     * obtain a new four vector
     * \f[ (\Delta t', \Delta\vec{x}'), \f]
     * with a non-zero time component (the collision happens simultaneously for
     * the two particles in the lab system, but not in the CM system). In order
     * for the avatar to be realised, we require that
     * \f[ |\Delta\vec{x}'| \leq \sqrt{\sigma/\pi}.\f]
     * Note that \f$|\Delta\vec{x}'|\leq|\Delta\vec{x}|\f$; thus, the condition
     * above is more restrictive than the check that we perform in
     * G4INCL::Propagation::StandardPropagationModel::generateBinaryCollisionAvatar.
     * In other words, the avatar generation cannot miss any physical collision
     * avatars.
     */
    ThreeVector minimumDistance = particle1->getPosition();
    minimumDistance -= particle2->getPosition();
    const G4double betaDotX = boostVector.dot(minimumDistance);
    const G4double minDist = Math::tenPi*(minimumDistance.mag2() + betaDotX*betaDotX / (1.-boostVector.mag2()));
    if(minDist > theCrossSection) {
      INCL_DEBUG("CM distance of approach is too small: " << minDist << ">" <<
        theCrossSection <<"; returning a NULL channel" << '\n');
      InteractionAvatar::restoreParticles();
      return NULL;
    }

//// NN
    if(particle1->isNucleon() && particle2->isNucleon()) {
      const G4double elasticCX = CrossSections::elastic(particle1, particle2);
      const G4double deltaProductionCX = CrossSections::NNToNDelta(particle1, particle2);
      const G4double onePiProductionCX = CrossSections::NNToxPiNN(1,particle1, particle2);
      const G4double twoPiProductionCX = CrossSections::NNToxPiNN(2,particle1, particle2);
      const G4double threePiProductionCX = CrossSections::NNToxPiNN(3,particle1, particle2);
      const G4double fourPiProductionCX = CrossSections::NNToxPiNN(4,particle1, particle2);
      const G4double etaProductionCX = CrossSections::NNToNNEtaExclu(particle1, particle2);
      const G4double etadeltaProductionCX = CrossSections::NNToNDeltaEta(particle1, particle2);
      const G4double etaonePiProductionCX = CrossSections::NNToNNEtaxPi(1,particle1, particle2);
      const G4double etatwoPiProductionCX = CrossSections::NNToNNEtaxPi(2,particle1, particle2);
      const G4double etathreePiProductionCX = CrossSections::NNToNNEtaxPi(3,particle1, particle2);
      const G4double etafourPiProductionCX = CrossSections::NNToNNEtaxPi(4,particle1, particle2);
      const G4double omegaProductionCX = CrossSections::NNToNNOmegaExclu(particle1, particle2);
      const G4double omegadeltaProductionCX = CrossSections::NNToNDeltaOmega(particle1, particle2);
      const G4double omegaonePiProductionCX = CrossSections::NNToNNOmegaxPi(1,particle1, particle2);
      const G4double omegatwoPiProductionCX = CrossSections::NNToNNOmegaxPi(2,particle1, particle2);
      const G4double omegathreePiProductionCX = CrossSections::NNToNNOmegaxPi(3,particle1, particle2);
      const G4double omegafourPiProductionCX = CrossSections::NNToNNOmegaxPi(4,particle1, particle2);
      const G4double totCX=CrossSections::total(particle1, particle2);

      const G4double rChannel=Random::shoot() * totCX;

      if(elasticCX > rChannel) {
// Elastic NN channel
        isElastic = true;
        INCL_DEBUG("NN interaction: elastic channel chosen" << '\n');
        return new ElasticChannel(particle1, particle2);
      } else if((elasticCX + deltaProductionCX) > rChannel) {
        isElastic = false;
// NN -> N Delta channel is chosen
        INCL_DEBUG("NN interaction: Delta channel chosen" << '\n');
        return new DeltaProductionChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX > rChannel) {
        isElastic = false;
// NN -> PiNN channel is chosen
        INCL_DEBUG("NN interaction: one Pion channel chosen" << '\n');
        return new NNToMultiPionsChannel(1,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX > rChannel) {
        isElastic = false;
// NN -> 2PiNN channel is chosen
        INCL_DEBUG("NN interaction: two Pions channel chosen" << '\n');
        return new NNToMultiPionsChannel(2,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX > rChannel) {
        isElastic = false;
// NN -> 3PiNN channel is chosen
        INCL_DEBUG("NN interaction: three Pions channel chosen" << '\n');
        return new NNToMultiPionsChannel(3,particle1, particle2);
      } else if (elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX > rChannel) {
		      isElastic = false;
// NN -> 4PiNN channel is chosen
		      INCL_DEBUG("NN interaction: four Pions channel chosen" << '\n');
		      return new NNToMultiPionsChannel(4,particle1, particle2);
      } else if (elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX > rChannel) {
       isElastic = false;
// NN -> NNEta channel is chosen
       INCL_DEBUG("NN interaction: Eta channel chosen" << '\n');
       return new NNToNNEtaChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX > rChannel) {
       isElastic = false;
// NN -> N Delta Eta channel is chosen
       INCL_DEBUG("NN interaction: Delta Eta channel chosen" << '\n');
       return new NDeltaEtaProductionChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX > rChannel) {
       isElastic = false;
// NN -> EtaPiNN channel is chosen
       INCL_DEBUG("NN interaction: Eta + one Pion channel chosen" << '\n');
       return new NNEtaToMultiPionsChannel(1,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX > rChannel) {
       isElastic = false;
// NN -> Eta2PiNN channel is chosen
       INCL_DEBUG("NN interaction: Eta + two Pions channel chosen" << '\n');
       return new NNEtaToMultiPionsChannel(2,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX > rChannel) {
       isElastic = false;
// NN -> Eta3PiNN channel is chosen
       INCL_DEBUG("NN interaction: Eta + three Pions channel chosen" << '\n');
       return new NNEtaToMultiPionsChannel(3,particle1, particle2);
      } else if (elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX > rChannel) {
       isElastic = false;
// NN -> Eta4PiNN channel is chosen
       INCL_DEBUG("NN interaction: Eta + four Pions channel chosen" << '\n');
       return new NNEtaToMultiPionsChannel(4,particle1, particle2);
      } else if (elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX > rChannel) {
       isElastic = false;
// NN -> NNOmega channel is chosen
       INCL_DEBUG("NN interaction: Omega channel chosen" << '\n');
       return new NNToNNOmegaChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX + omegadeltaProductionCX > rChannel) {
       isElastic = false;
// NN -> N Delta Omega channel is chosen
       INCL_DEBUG("NN interaction: Delta Omega channel chosen" << '\n');
       return new NDeltaOmegaProductionChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX + omegadeltaProductionCX + omegaonePiProductionCX > rChannel) {
       isElastic = false;
// NN -> OmegaPiNN channel is chosen
       INCL_DEBUG("NN interaction: Omega + one Pion channel chosen" << '\n');
       return new NNOmegaToMultiPionsChannel(1,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX + omegadeltaProductionCX + omegaonePiProductionCX + omegatwoPiProductionCX > rChannel) {
       isElastic = false;
// NN -> Omega2PiNN channel is chosen
       INCL_DEBUG("NN interaction: Omega + two Pions channel chosen" << '\n');
       return new NNOmegaToMultiPionsChannel(2,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX + omegadeltaProductionCX + omegaonePiProductionCX + omegatwoPiProductionCX + omegathreePiProductionCX > rChannel) {
       isElastic = false;
// NN -> Omega3PiNN channel is chosen
       INCL_DEBUG("NN interaction: Omega + three Pions channel chosen" << '\n');
       return new NNOmegaToMultiPionsChannel(3,particle1, particle2);
      } else if (elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + fourPiProductionCX + etaProductionCX + etadeltaProductionCX + etaonePiProductionCX + etatwoPiProductionCX + etathreePiProductionCX + etafourPiProductionCX + omegaProductionCX + omegadeltaProductionCX + omegaonePiProductionCX + omegatwoPiProductionCX + omegathreePiProductionCX + omegafourPiProductionCX > rChannel) {
       isElastic = false;
// NN -> Omega4PiNN channel is chosen
       INCL_DEBUG("NN interaction: Omega + four Pions channel chosen" << '\n');
       return new NNOmegaToMultiPionsChannel(4,particle1, particle2);
      } else {
        INCL_WARN("inconsistency within the NN Cross Sections (sum!=inelastic)" << '\n');
        if(omegafourPiProductionCX>0.) {
         INCL_WARN("Returning an Omega + four Pions channel" << '\n');
         isElastic = false;
         return new NNOmegaToMultiPionsChannel(4,particle1, particle2);
        } else if(omegathreePiProductionCX>0.) {
         INCL_WARN("Returning an Omega + three Pions channel" << '\n');
         isElastic = false;
         return new NNOmegaToMultiPionsChannel(3,particle1, particle2);
        } else if(omegatwoPiProductionCX>0.) {
         INCL_WARN("Returning an Omega + two Pions channel" << '\n');
         isElastic = false;
         return new NNOmegaToMultiPionsChannel(2,particle1, particle2);
        } else if(omegaonePiProductionCX>0.) {
         INCL_WARN("Returning an Omega + one Pion channel" << '\n');
         isElastic = false;
         return new NNOmegaToMultiPionsChannel(1,particle1, particle2);
        } else if(omegadeltaProductionCX>0.) {
         INCL_WARN("Returning an Omega + Delta channel" << '\n');
         isElastic = false;
         return new NDeltaOmegaProductionChannel(particle1, particle2);
        } else if(omegaProductionCX>0.) {
         INCL_WARN("Returning an Omega channel" << '\n');
         isElastic = false;
         return new NNToNNOmegaChannel(particle1, particle2);
        } else if(etafourPiProductionCX>0.) {
         INCL_WARN("Returning an Eta + four Pions channel" << '\n');
         isElastic = false;
         return new NNEtaToMultiPionsChannel(3,particle1, particle2);
        } else if(etathreePiProductionCX>0.) {
         INCL_WARN("Returning an Eta + threev channel" << '\n');
         isElastic = false;
         return new NNEtaToMultiPionsChannel(3,particle1, particle2);
        } else if(etatwoPiProductionCX>0.) {
         INCL_WARN("Returning an Eta + two Pions channel" << '\n');
         isElastic = false;
         return new NNEtaToMultiPionsChannel(2,particle1, particle2);
        } else if(etaonePiProductionCX>0.) {
         INCL_WARN("Returning an Eta + one Pion channel" << '\n');
         isElastic = false;
         return new NNEtaToMultiPionsChannel(1,particle1, particle2);
        } else if(etadeltaProductionCX>0.) {
         INCL_WARN("Returning an Eta + Delta channel" << '\n');
         isElastic = false;
         return new NDeltaEtaProductionChannel(particle1, particle2);
        } else if(etaProductionCX>0.) {
         INCL_WARN("Returning an Eta channel" << '\n');
         isElastic = false;
         return new NNToNNEtaChannel(particle1, particle2);
        } else if(fourPiProductionCX>0.) {
		        INCL_WARN("Returning a 4pi channel" << '\n');
		        isElastic = false;
		        return new NNToMultiPionsChannel(4,particle1, particle2);
        } else if(threePiProductionCX>0.) {
		        INCL_WARN("Returning a 3pi channel" << '\n');
		        isElastic = false;
		        return new NNToMultiPionsChannel(3,particle1, particle2);
        } else if(twoPiProductionCX>0.) {
          INCL_WARN("Returning a 2pi channel" << '\n');
          isElastic = false;
          return new NNToMultiPionsChannel(2,particle1, particle2);
        } else if(onePiProductionCX>0.) {
          INCL_WARN("Returning a 1pi channel" << '\n');
          isElastic = false;
          return new NNToMultiPionsChannel(1,particle1, particle2);
        } else if(deltaProductionCX>0.) {
          INCL_WARN("Returning a delta-production channel" << '\n');
          isElastic = false;
          return new DeltaProductionChannel(particle1, particle2);
        } else {
          INCL_WARN("Returning an elastic channel" << '\n');
          isElastic = true;
          return new ElasticChannel(particle1, particle2);
        }
      }

//// NDelta
    } else if((particle1->isNucleon() && particle2->isDelta()) ||
	             (particle1->isDelta() && particle2->isNucleon())) {
          G4double elasticCX = CrossSections::elastic(particle1, particle2);
          G4double recombinationCX = CrossSections::NDeltaToNN(particle1, particle2);

          if(elasticCX/(elasticCX + recombinationCX) < Random::shoot()) {
            isElastic = false;
          } else
            isElastic = true;

          if(isElastic) {
// Elastic N Delta channel
												INCL_DEBUG("NDelta interaction: elastic channel chosen" << '\n');
												return new ElasticChannel(particle1, particle2);
          } else { // Recombination
// N Delta -> NN channel is chosen
												  INCL_DEBUG("NDelta interaction: recombination channel chosen" << '\n');
              return new RecombinationChannel(particle1, particle2);
            }

//// DeltaDelta
    } else if(particle1->isDelta() && particle2->isDelta()) {
        isElastic = true;
        INCL_DEBUG("DeltaDelta interaction: elastic channel chosen" << '\n');
        return new ElasticChannel(particle1, particle2);

//// PiN
    } else if(isPiN) {
      const G4double elasticCX = CrossSections::elastic(particle1, particle2);
      const G4double deltaProductionCX = CrossSections::piNToDelta(particle1, particle2);
      const G4double onePiProductionCX = CrossSections::piNToxPiN(2,particle1, particle2);
      const G4double twoPiProductionCX = CrossSections::piNToxPiN(3,particle1, particle2);
	     const G4double threePiProductionCX = CrossSections::piNToxPiN(4,particle1, particle2);
	     const G4double etaProductionCX = CrossSections::piNToEtaN(particle1, particle2);
	     const G4double omegaProductionCX = CrossSections::piNToOmegaN(particle1, particle2);
      const G4double totCX=CrossSections::total(particle1, particle2);
// assert(std::fabs(totCX-elasticCX-deltaProductionCX-onePiProductionCX-twoPiProductionCX-threePiProductionCX-etaProductionCX-omegaProductionCX)<1.);

      const G4double rChannel=Random::shoot() * totCX;

      if(elasticCX > rChannel) {
// Elastic PiN channel
        isElastic = true;
        INCL_DEBUG("PiN interaction: elastic channel chosen" << '\n');
        return new PiNElasticChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX > rChannel) {
        isElastic = false;
// PiN -> Delta channel is chosen
        INCL_DEBUG("PiN interaction: Delta channel chosen" << '\n');
        return new PiNToDeltaChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX > rChannel) {
        isElastic = false;
// PiN -> PiNPi channel is chosen
        INCL_DEBUG("PiN interaction: one Pion channel chosen" << '\n');
        return new PiNToMultiPionsChannel(2,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX > rChannel) {
        isElastic = false;
// PiN -> PiN2Pi channel is chosen
        INCL_DEBUG("PiN interaction: two Pions channel chosen" << '\n');
        return new PiNToMultiPionsChannel(3,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX > rChannel) {
		      isElastic = false;
// PiN -> PiN3Pi channel is chosen
		      INCL_DEBUG("PiN interaction: three Pions channel chosen" << '\n');
		      return new PiNToMultiPionsChannel(4,particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + etaProductionCX > rChannel) {
		      isElastic = false;
// PiN -> EtaN channel is chosen
		      INCL_DEBUG("PiN interaction: Eta channel chosen" << '\n');
		      return new PiNToEtaChannel(particle1, particle2);
      } else if(elasticCX + deltaProductionCX + onePiProductionCX + twoPiProductionCX + threePiProductionCX + etaProductionCX+ omegaProductionCX > rChannel) {
		      isElastic = false;
// PiN -> OmegaN channel is chosen
	      	INCL_DEBUG("PiN interaction: Omega channel chosen" << '\n');
	      	return new PiNToOmegaChannel(particle1, particle2);
      } 
						
						else {
        INCL_WARN("inconsistency within the PiN Cross Sections (sum!=inelastic)" << '\n');
        if(omegaProductionCX>0.) {
          INCL_WARN("Returning a Omega channel" << '\n');
          isElastic = false;
          return new PiNToOmegaChannel(particle1, particle2);
        } else if(etaProductionCX>0.) {
		        INCL_WARN("Returning a Eta channel" << '\n');
										isElastic = false;
		        return new PiNToEtaChannel(particle1, particle2);
        } else if(threePiProductionCX>0.) {
		        INCL_WARN("Returning a 3pi channel" << '\n');
		        isElastic = false;
		        return new PiNToMultiPionsChannel(4,particle1, particle2);
        } else if(twoPiProductionCX>0.) {
		        INCL_WARN("Returning a 2pi channel" << '\n');
		        isElastic = false;
		        return new PiNToMultiPionsChannel(3,particle1, particle2);
        } else if(onePiProductionCX>0.) {
          INCL_WARN("Returning a 1pi channel" << '\n');
          isElastic = false;
          return new PiNToMultiPionsChannel(2,particle1, particle2);
        } else if(deltaProductionCX>0.) {
          INCL_WARN("Returning a delta-production channel" << '\n');
          isElastic = false;
          return new PiNToDeltaChannel(particle1, particle2);
        } else {
          INCL_WARN("Returning an elastic channel" << '\n');
          isElastic = true;
          return new PiNElasticChannel(particle1, particle2);
        }
      }
    } else if ((particle1->isNucleon() && particle2->isEta()) || (particle2->isNucleon() && particle1->isEta())) {
//// EtaN

					const G4double elasticCX = CrossSections::elastic(particle1, particle2);
					const G4double onePiProductionCX = CrossSections::etaNToPiN(particle1, particle2);
					const G4double twoPiProductionCX = CrossSections::etaNToPiPiN(particle1, particle2);
					const G4double totCX=CrossSections::total(particle1, particle2);
// assert(std::fabs(totCX-elasticCX-onePiProductionCX-twoPiProductionCX)<1.);
					
					const G4double rChannel=Random::shoot() * totCX;
										
					if(elasticCX > rChannel) {
// Elastic EtaN channel
						isElastic = true;
						INCL_DEBUG("EtaN interaction: elastic channel chosen" << '\n');
						return new EtaNElasticChannel(particle1, particle2);
					} else if(elasticCX + onePiProductionCX > rChannel) {
						isElastic = false;
// EtaN -> EtaPiN channel is chosen
						INCL_DEBUG("EtaN interaction: PiN channel chosen" << '\n');
						return new EtaNToPiNChannel(particle1, particle2);
					} else if(elasticCX + onePiProductionCX + twoPiProductionCX > rChannel) {
						isElastic = false;
// EtaN -> EtaPiPiN channel is chosen
						INCL_DEBUG("EtaN interaction: PiPiN channel chosen" << '\n');
						return new EtaNToPiPiNChannel(particle1, particle2);
					}

					else {
						INCL_WARN("inconsistency within the EtaN Cross Sections (sum!=inelastic)" << '\n');
						if(twoPiProductionCX>0.) {
							INCL_WARN("Returning a PiPiN channel" << '\n');
							isElastic = false;
							return new EtaNToPiPiNChannel(particle1, particle2);
						} else if(onePiProductionCX>0.) {
							INCL_WARN("Returning a PiN channel" << '\n');
							isElastic = false;
							return new EtaNToPiNChannel(particle1, particle2);
						} else {
							INCL_WARN("Returning an elastic channel" << '\n');
							isElastic = true;
							return new EtaNElasticChannel(particle1, particle2);
						}
					}
									
				} else if ((particle1->isNucleon() && particle2->isOmega()) || (particle2->isNucleon() && particle1->isOmega())) {
//// OmegaN
     
					const G4double elasticCX = CrossSections::elastic(particle1, particle2);
					const G4double onePiProductionCX = CrossSections::omegaNToPiN(particle1, particle2);
					const G4double twoPiProductionCX = CrossSections::omegaNToPiPiN(particle1, particle2);
					const G4double totCX=CrossSections::total(particle1, particle2);
// assert(std::fabs(totCX-elasticCX-onePiProductionCX-twoPiProductionCX)<1.);
					
					const G4double rChannel=Random::shoot() * totCX;
     
					if(elasticCX > rChannel) {
// Elastic OmegaN channel
						isElastic = true;
						INCL_DEBUG("OmegaN interaction: elastic channel chosen" << '\n');
						return new OmegaNElasticChannel(particle1, particle2);
					} else if(elasticCX + onePiProductionCX > rChannel) {
						isElastic = false;
// OmegaN -> OmegaPiN channel is chosen
		    INCL_DEBUG("OmegaN interaction: PiN channel chosen" << '\n');
		    return new OmegaNToPiNChannel(particle1, particle2);
					} else if(elasticCX + onePiProductionCX + twoPiProductionCX > rChannel) {
						isElastic = false;
// OmegaN -> OmegaPiPiN channel is chosen
						INCL_DEBUG("OmegaN interaction: PiPiN channel chosen" << '\n');
						return new OmegaNToPiPiNChannel(particle1, particle2);
					}
     
					else {
						INCL_WARN("inconsistency within the OmegaN Cross Sections (sum!=inelastic)" << '\n');
						if(twoPiProductionCX>0.) {
							INCL_WARN("Returning a PiPiN channel" << '\n');
							isElastic = false;
							return new OmegaNToPiPiNChannel(particle1, particle2);
						} else if(onePiProductionCX>0.) {
							INCL_WARN("Returning a PiN channel" << '\n');
							isElastic = false;
							return new OmegaNToPiNChannel(particle1, particle2);
						} else {
							INCL_WARN("Returning an elastic channel" << '\n');
							isElastic = true;
							return new OmegaNElasticChannel(particle1, particle2);
						}
					}
    }	
	else {
      INCL_DEBUG("BinaryCollisionAvatar can only handle nucleons (for the moment)."
	      << '\n'
	      << particle1->print()
	      << '\n'
	      << particle2->print()
	      << '\n');
      InteractionAvatar::restoreParticles();
      return NULL;
    }
  }

  void BinaryCollisionAvatar::preInteraction() {
    isParticle1Spectator = particle1->isTargetSpectator();
    isParticle2Spectator = particle2->isTargetSpectator();
    InteractionAvatar::preInteraction();
  }

  void BinaryCollisionAvatar::postInteraction(FinalState *fs) {
    // Call the postInteraction method of the parent class
    // (provides Pauli blocking and enforces energy conservation)
    InteractionAvatar::postInteraction(fs);

    switch(fs->getValidity()) {
      case PauliBlockedFS:
        theNucleus->getStore()->getBook().incrementBlockedCollisions();
        break;
      case NoEnergyConservationFS:
      case ParticleBelowFermiFS:
      case ParticleBelowZeroFS:
        break;
      case ValidFS:
        Book &theBook = theNucleus->getStore()->getBook();
        theBook.incrementAcceptedCollisions();
        if(theBook.getAcceptedCollisions() == 1) {
          // Store time and cross section of the first collision
          G4double t = theBook.getCurrentTime();
          theBook.setFirstCollisionTime(t);
          theBook.setFirstCollisionXSec(oldXSec);

          // Store position and momentum of the spectator on the first
          // collision
          if((isParticle1Spectator && isParticle2Spectator) || (!isParticle1Spectator && !isParticle2Spectator)) {
            INCL_ERROR("First collision must be within a target spectator and a non-target spectator");
          }
          if(isParticle1Spectator) {
            theBook.setFirstCollisionSpectatorPosition(backupParticle1->getPosition().mag());
            theBook.setFirstCollisionSpectatorMomentum(backupParticle1->getMomentum().mag());
          } else {
            theBook.setFirstCollisionSpectatorPosition(backupParticle2->getPosition().mag());
            theBook.setFirstCollisionSpectatorMomentum(backupParticle2->getMomentum().mag());
          }

          // Store the elasticity of the first collision
          theBook.setFirstCollisionIsElastic(isElastic);
        }
    }
    return;
  }

  std::string BinaryCollisionAvatar::dump() const {
    std::stringstream ss;
    ss << "(avatar " << theTime <<" 'nn-collision" << '\n'
      << "(list " << '\n'
      << particle1->dump()
      << particle2->dump()
      << "))" << '\n';
    return ss.str();
  }

}