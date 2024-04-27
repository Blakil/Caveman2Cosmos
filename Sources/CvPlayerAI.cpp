// playerAI.cpp

#include "CvGameCoreDLL.h"
#include "CvArea.h"
#include "CvBonusInfo.h"
#include "CvBuildingInfo.h"
#include "CvCity.h"
#include "CvCityAI.h"
#include "CvDeal.h"
#include "CvDiploParameters.h"
#include "CvEventReporter.h"
#include "CvGameAI.h"
#include "CvGlobals.h"
#include "CvImprovementInfo.h"
#include "CvBonusInfo.h"
#include "CvInfos.h"
#include "CvUnitCombatInfo.h"
#include "CvTraitInfo.h"
#include "CvInitCore.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvPathGenerator.h"
#include "CvPlayerAI.h"
#include "CvPopupInfo.h"
#include "CvPython.h"
#include "CvSelectionGroup.h"
#include "CvTeamAI.h"
#include "CvUnit.h"
#include "CvUnitSelectionCriteria.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"
#include "FAStarNode.h"

// Plot danger cache
//#define DANGER_RANGE						(4)

#define GREATER_FOUND_RANGE			(5)
#define CIVIC_CHANGE_DELAY			(25)
#define RELIGION_CHANGE_DELAY		(15)

//	Koshling - save flag indicating this player has no data in the save as they have never been alive
#define	PLAYERAI_UI_FLAG_OMITTED 4

//	Koshling - to try to normalize the new tech building evaluation to the same magnitude as the old
//	(so that it doesn't change it's value as a component relative to other factors) a multiplier is needed
#define	BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER	30

// statics

CvPlayerAI* CvPlayerAI::m_aPlayers = NULL;

void CvPlayerAI::initStatics()
{
	PROFILE_EXTRA_FUNC();
	m_aPlayers = new CvPlayerAI[MAX_PLAYERS];
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aPlayers[iI].m_eID = ((PlayerTypes)iI);
	}
}

void CvPlayerAI::freeStatics()
{
	SAFE_DELETE_ARRAY(m_aPlayers);
}

bool CvPlayerAI::areStaticsInitialized()
{
	if (m_aPlayers == NULL)
	{
		return false;
	}

	return true;
}

DllExport CvPlayerAI& CvPlayerAI::getPlayerNonInl(PlayerTypes ePlayer)
{
	if (ePlayer <= NO_PLAYER || ePlayer >= MAX_PLAYERS)
	{
		return getPlayer(BARBARIAN_PLAYER);
	}
	return getPlayer(ePlayer);
}

// Public Functions...

CvPlayerAI::CvPlayerAI()
{
	PROFILE_EXTRA_FUNC();
	m_aiNumTrainAIUnits = new int[NUM_UNITAI_TYPES];
	m_aiNumAIUnits = new int[NUM_UNITAI_TYPES];
	m_aiSameReligionCounter = new int[MAX_PLAYERS];
	m_aiDifferentReligionCounter = new int[MAX_PLAYERS];
	m_aiFavoriteCivicCounter = new int[MAX_PLAYERS];
	m_aiBonusTradeCounter = new int[MAX_PLAYERS];
	m_aiPeacetimeTradeValue = new int[MAX_PLAYERS];
	m_aiPeacetimeGrantValue = new int[MAX_PLAYERS];
	m_aiGoldTradedTo = new int[MAX_PLAYERS];
	m_aiAttitudeExtra = new int[MAX_PLAYERS];

	m_abFirstContact = new bool[MAX_PLAYERS];

	m_aaiContactTimer = new int* [MAX_PLAYERS];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_aaiContactTimer[i] = new int[NUM_CONTACT_TYPES];
	}

	// @SAVEBREAK - change to MAX_PC_PLAYERS, no need to have memory of NPC actions.
	m_aaiMemoryCount = new int* [MAX_PLAYERS];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		m_aaiMemoryCount[i] = new int[NUM_MEMORY_TYPES];
	}
	// !SAVEBREAK

	m_aiAverageYieldMultiplier = new int[NUM_YIELD_TYPES];
	m_aiAverageCommerceMultiplier = new int[NUM_COMMERCE_TYPES];
	m_aiAverageCommerceExchange = new int[NUM_COMMERCE_TYPES];

	m_aiBonusValue = NULL;
	m_aiTradeBonusValue = NULL;
	m_abNonTradeBonusCalculated = NULL;
	m_aiUnitWeights = NULL;
	m_aiUnitCombatWeights = NULL;
	m_aiCloseBordersAttitudeCache = new int[MAX_PLAYERS];

	m_aiCivicValueCache = NULL;

	m_aiAttitudeCache = new int[MAX_PLAYERS];

	// Toffer - Transient Caches
	m_bonusClassRevealed = NULL;
	m_bonusClassUnrevealed = NULL;
	m_bonusClassHave = NULL;

	m_canTrainSettler = false;

	AI_reset(true);
}


CvPlayerAI::~CvPlayerAI()
{
	AI_uninit();

	SAFE_DELETE_ARRAY(m_aiNumTrainAIUnits);
	SAFE_DELETE_ARRAY(m_aiNumAIUnits);
	SAFE_DELETE_ARRAY(m_aiSameReligionCounter);
	SAFE_DELETE_ARRAY(m_aiDifferentReligionCounter);
	SAFE_DELETE_ARRAY(m_aiFavoriteCivicCounter);
	SAFE_DELETE_ARRAY(m_aiBonusTradeCounter);
	SAFE_DELETE_ARRAY(m_aiPeacetimeTradeValue);
	SAFE_DELETE_ARRAY(m_aiPeacetimeGrantValue);
	SAFE_DELETE_ARRAY(m_aiGoldTradedTo);
	SAFE_DELETE_ARRAY(m_aiAttitudeExtra);
	SAFE_DELETE_ARRAY(m_abFirstContact);
	SAFE_DELETE_ARRAY2(m_aaiContactTimer, MAX_PLAYERS);
	SAFE_DELETE_ARRAY2(m_aaiMemoryCount, MAX_PLAYERS);
	SAFE_DELETE_ARRAY(m_aiAverageYieldMultiplier);
	SAFE_DELETE_ARRAY(m_aiAverageCommerceMultiplier);
	SAFE_DELETE_ARRAY(m_aiAverageCommerceExchange);
	SAFE_DELETE_ARRAY(m_aiCloseBordersAttitudeCache);
	SAFE_DELETE_ARRAY(m_aiAttitudeCache);
	// Toffer - Transient Caches
	SAFE_DELETE_ARRAY(m_bonusClassRevealed);
	SAFE_DELETE_ARRAY(m_bonusClassUnrevealed);
	SAFE_DELETE_ARRAY(m_bonusClassHave);
}


void CvPlayerAI::AI_init()
{
	AI_reset(false);

	//--------------------------------
	// Init other game data
	if ((GC.getInitCore().getSlotStatus(getID()) == SS_TAKEN) || (GC.getInitCore().getSlotStatus(getID()) == SS_COMPUTER))
	{
		FAssert(getPersonalityType() != NO_LEADER);
		AI_setPeaceWeight(GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() + GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getPeaceWeightRand(), "AI Peace Weight"));
		AI_setEspionageWeight(GC.getLeaderHeadInfo(getPersonalityType()).getEspionageWeight());
		//AI_setCivicTimer(((getMaxAnarchyTurns() == 0) ? (GC.getDefineINT("MIN_REVOLUTION_TURNS") * 2) : CIVIC_CHANGE_DELAY) / 2);
		AI_setReligionTimer(1);
		AI_setCivicTimer(1);
	}
}


void CvPlayerAI::AI_uninit()
{
	SAFE_DELETE_ARRAY(m_aiBonusValue);
	SAFE_DELETE_ARRAY(m_aiTradeBonusValue);
	SAFE_DELETE_ARRAY(m_abNonTradeBonusCalculated);
	SAFE_DELETE_ARRAY(m_aiUnitWeights);
	SAFE_DELETE_ARRAY(m_aiUnitCombatWeights);
	SAFE_DELETE_ARRAY(m_aiCivicValueCache);
}


void CvPlayerAI::AI_reset(bool bConstructor)
{
	PROFILE_EXTRA_FUNC();
	AI_uninit();

	m_iPeaceWeight = 0;
	m_iEspionageWeight = 0;
	m_iAttackOddsChange = 0;
	m_iCivicTimer = 0;
	m_iReligionTimer = 0;
	m_iExtraGoldTarget = 0;

	bUnitRecalcNeeded = false;
	m_bCitySitesNotCalculated = true;
	m_iCityGrowthValueBase = -1;
	m_turnsSinceLastRevolution = 50;	//	Start off at the functional max (larger makes no diff)
	m_iCivicSwitchMinDeltaThreshold = 0;

	m_eBestResearchTarget = NO_TECH;
	m_cachedTechValues.clear();

	if (bConstructor || getNumUnits() == 0)
	{
		for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
		{
			m_aiNumTrainAIUnits[iI] = 0;
			m_aiNumAIUnits[iI] = 0;
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiSameReligionCounter[iI] = 0;
		m_aiDifferentReligionCounter[iI] = 0;
		m_aiFavoriteCivicCounter[iI] = 0;
		m_aiBonusTradeCounter[iI] = 0;
		m_aiPeacetimeTradeValue[iI] = 0;
		m_aiPeacetimeGrantValue[iI] = 0;
		m_aiGoldTradedTo[iI] = 0;
		m_aiAttitudeExtra[iI] = 0;
		m_abFirstContact[iI] = false;
		for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
		{
			m_aaiContactTimer[iI][iJ] = 0;
		}
		for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
		{
			m_aaiMemoryCount[iI][iJ] = 0;
		}

		if (!bConstructor && getID() != NO_PLAYER)
		{
			PlayerTypes eLoopPlayer = (PlayerTypes)iI;
			CvPlayerAI& kLoopPlayer = GET_PLAYER(eLoopPlayer);
			kLoopPlayer.m_aiSameReligionCounter[getID()] = 0;
			kLoopPlayer.m_aiDifferentReligionCounter[getID()] = 0;
			kLoopPlayer.m_aiFavoriteCivicCounter[getID()] = 0;
			kLoopPlayer.m_aiBonusTradeCounter[getID()] = 0;
			kLoopPlayer.m_aiPeacetimeTradeValue[getID()] = 0;
			kLoopPlayer.m_aiPeacetimeGrantValue[getID()] = 0;
			kLoopPlayer.m_aiGoldTradedTo[getID()] = 0;
			kLoopPlayer.m_aiAttitudeExtra[getID()] = 0;
			kLoopPlayer.m_abFirstContact[getID()] = false;
			for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
			{
				kLoopPlayer.m_aaiContactTimer[getID()][iJ] = 0;
			}
			for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
			{
				kLoopPlayer.m_aaiMemoryCount[getID()][iJ] = 0;
			}
		}
	}

	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		m_aiAverageYieldMultiplier[iI] = 0;
	}
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		m_aiAverageCommerceMultiplier[iI] = 0;
		m_aiAverageCommerceExchange[iI] = 0;
	}
	m_iAverageGreatPeopleMultiplier = 0;
	m_iAveragesCacheTurn = -1;

	m_iStrategyHash = 0;
	m_iStrategyHashCacheTurn = -1;

	m_iStrategyRand = 0;

	m_iVictoryStrategyHash = 0;
	m_iVictoryStrategyHashCacheTurn = -1;

	m_bWasFinancialTrouble = false;
	m_iTurnLastProductionDirty = -1;

	m_iUpgradeUnitsCacheTurn = -1;
	m_iUpgradeUnitsCachedExpThreshold = 0;
	m_iUpgradeUnitsCachedGold = 0;

	m_iMilitaryProductionCityCount = -1;
	m_iNavalMilitaryProductionCityCount = -1;

	FAssert(m_aiCivicValueCache == NULL);
	m_aiCivicValueCache = new int[GC.getNumCivicInfos() * 2];

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}
	m_aiAICitySites.clear();

	FAssert(m_aiBonusValue == NULL);
	FAssert(m_aiTradeBonusValue == NULL);
	m_aiBonusValue = new int[GC.getNumBonusInfos()];
	m_aiTradeBonusValue = new int[GC.getNumBonusInfos()];
	m_abNonTradeBonusCalculated = new bool[GC.getNumBonusInfos()];

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		m_aiBonusValue[iI] = -1;
		m_aiTradeBonusValue[iI] = -1;
		m_abNonTradeBonusCalculated[iI] = false;
	}

	FAssert(m_aiUnitWeights == NULL);
	m_aiUnitWeights = new int[GC.getNumUnitInfos()];

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		m_aiUnitWeights[iI] = 0;
	}

	FAssert(m_aiUnitCombatWeights == NULL);
	m_aiUnitCombatWeights = new int[GC.getNumUnitCombatInfos()];
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		m_aiUnitCombatWeights[iI] = 0;
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		m_aiCloseBordersAttitudeCache[iI] = 0;

		if (!bConstructor && getID() != NO_PLAYER)
		{
			GET_PLAYER((PlayerTypes)iI).m_aiCloseBordersAttitudeCache[getID()] = 0;
		}
		m_aiAttitudeCache[iI] = MAX_INT;

		if (!bConstructor && getID() != NO_PLAYER)
		{
			GET_PLAYER((PlayerTypes)iI).m_aiAttitudeCache[getID()] = MAX_INT;
		}
	}
	m_missionTargetCache.clear();

	if (!bConstructor && m_bonusClassRevealed == NULL)
	{
		// Can assume none of them are initialized if one of them isn't.
		FAssertMsg(m_bonusClassUnrevealed == NULL, "Memory leak");
		FAssertMsg(m_bonusClassHave == NULL, "Memory leak");
		m_bonusClassRevealed = new int[GC.getNumBonusClassInfos()];
		m_bonusClassUnrevealed = new int[GC.getNumBonusClassInfos()];
		m_bonusClassHave = new int[GC.getNumBonusClassInfos()];
	}
	m_iBonusClassTallyCachedTurn = -1;
}


int CvPlayerAI::AI_getFlavorValue(FlavorTypes eFlavor) const
{
	FAssertMsg((getPersonalityType() >= 0), "getPersonalityType() is less than zero");
	FAssertMsg((eFlavor >= 0), "eFlavor is less than zero");
	return GC.getLeaderHeadInfo(getPersonalityType()).getFlavorValue(eFlavor);
}


void CvPlayerAI::AI_doTurnPre()
{
	PROFILE_FUNC();

	m_cachedTechValues.clear();

	FAssertMsg(getPersonalityType() != NO_LEADER, "getPersonalityType() is not expected to be equal with NO_LEADER");
	FAssertMsg(getLeaderType() != NO_LEADER, "getLeaderType() is not expected to be equal with NO_LEADER");
	FAssertMsg(getCivilizationType() != NO_CIVILIZATION, "getCivilizationType() is not expected to be equal with NO_CIVILIZATION");

	// AIAndy: Calculate the strategy rand if needed
	AI_calculateStrategyRand();

	if (bUnitRecalcNeeded)
	{
		AI_recalculateUnitCounts();
	}

	//	Force recalculation of the mission target cache each turn for
	//	reliabilty reasons (more robust to bugs)
	m_missionTargetCache.clear();

#ifdef _DEBUG
	//	Validate AI unit counts
	{
		int iMilitary = 0;

		foreach_(const CvArea * pLoopArea, GC.getMap().areas())
		{
			for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
			{
				int iCount = 0;

				foreach_(const CvUnit * pLoopUnit, units())
				{
					if (pLoopUnit->isTempUnit()) continue;

					const UnitAITypes eAIType = pLoopUnit->AI_getUnitAIType();

					if ((UnitAITypes)iI == eAIType && pLoopUnit->area() == pLoopArea)
					{
						iCount++;

						if (GC.getUnitInfo(pLoopUnit->getUnitType()).isMilitarySupport())
						{
							iMilitary++;
						}
					}
				}

				if (iCount != 0)
				{
					OutputDebugString(CvString::format("Player %d, Area %d, unitAI %s count=%d\n", getID(), pLoopArea->getID(), GC.getUnitAIInfo((UnitAITypes)iI).getType(), iCount).c_str());
				}

				if (iCount != pLoopArea->getNumAIUnits(getID(), (UnitAITypes)iI))
				{
					FErrorMsg("UnitAI miscount");
				}
			}
		}

		FAssert(iMilitary == getNumMilitaryUnits());
	}
#endif

	AI_invalidateCloseBordersAttitudeCache();

	AI_doCounter();

	AI_invalidateAttitudeCache();

	m_numBuildingsNeeded.clear();

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}

	AI_updateBonusValue();

	AI_doEnemyUnitData();

	if (isHumanPlayer())
	{
		return;
	}

	//	Mark previous yield data as stale
#ifdef YIELD_VALUE_CACHING
	algo::for_each(cities(), CvCity::fn::ClearYieldValueCache());
#endif

	if (!isNPC() && getCurrentResearch() == NO_TECH)
	{
		PROFILE_FUNC()
			AI_chooseResearch();
		AI_forceUpdateStrategies(); //to account for current research.
	}

	AI_doCommerce();

	AI_doMilitary();

	AI_doCivics();

	AI_doReligion();

	AI_setPushReligiousVictory();
	AI_setConsiderReligiousVictory();
	AI_setHasInquisitionTarget();

	AI_doCheckFinancialTrouble();

	AI_doMilitaryProductionCity();

	if (isNPC())
	{
		return;
	}

	if (isMinorCiv())
	{
		return;
	}
}


void CvPlayerAI::AI_doTurnPost()
{
	PROFILE_FUNC();

	if (isHumanPlayer() || isNPC() || isMinorCiv())
	{
		return;
	}

	AI_doDiplo();

	for (int i = 0; i < GC.getNumVictoryInfos(); ++i)
	{
		AI_launch((VictoryTypes)i);
	}
}


void CvPlayerAI::AI_doTurnUnitsPre()
{
	PROFILE_FUNC();

	//	Clear cached defensive status info on each city
	algo::for_each(cities(), CvCity::fn::AI_preUnitTurn());

#ifdef PLOT_DANGER_CACHING
	//	Clear plot danger cache
	plotDangerCache.clear();
#endif

	AI_updateFoundValues(true);

	if (GC.getGame().getSorenRandNum(8, "AI Update Area Targets") == 0) // XXX personality???
	{
		AI_updateAreaTargets();
	}

	if (isHumanPlayer())
	{
		return;
	}

	if (isNPC())
	{
		return;
	}

	//k-mod uncommented
	if (AI_isDoStrategy(AI_STRATEGY_CRUSH))
	{
		AI_convertUnitAITypesForCrush();
	}
}

// WIP scoring refactor
namespace {
	struct UnitAndUpgrade
	{
		UnitAndUpgrade() {}
		UnitAndUpgrade(const UnitAndUpgrade& other) : unit(other.unit), upgrade(other.upgrade) {}
		UnitAndUpgrade(CvUnit* unit, UnitTypes upgrade) : unit(unit), upgrade(upgrade) {}
		CvUnit* unit;
		UnitTypes upgrade;
	};
	typedef scoring::item_score< bst::optional< UnitAndUpgrade > > UnitUpgradeScore;

	UnitUpgradeScore scoreUpgradePrice(CvUnit* unit, int upgradeUnit)
	{
		if (unit->canUpgrade((UnitTypes)upgradeUnit))
		{
			return UnitUpgradeScore(UnitAndUpgrade(unit, (UnitTypes)upgradeUnit), unit->upgradePrice((UnitTypes)upgradeUnit));
		}
		return UnitUpgradeScore();
	}

	struct MostExpensiveUpgrade : std::unary_function<CvUnit*, UnitUpgradeScore>
	{
		MostExpensiveUpgrade() {}

		UnitUpgradeScore operator()(CvUnit* unit) const
		{
			const std::vector<int>& upgradeChain = GC.getUnitInfo(unit->getUnitType()).getUnitUpgradeChain();
			bst::function<UnitUpgradeScore(int upgradeUnit)> sdsd = bind(scoreUpgradePrice, unit, _1);
			return algo::max_element(upgradeChain | transformed(sdsd)).get_value_or(UnitUpgradeScore());
		}
	};
}

void CvPlayerAI::AI_doTurnUnitsPost()
{
	PROFILE_FUNC();

	if (!isHumanPlayer() || isOption(PLAYEROPTION_AUTO_PROMOTION))
	{
		// Copy units as we will be removing and adding some now.
		foreach_(CvUnit * unit, units_safe() | filtered(CvUnit::fn::isPromotionReady()))
		{
			unit->AI_promote();
		}
	}

	if (isHumanPlayer())
	{
		int iMinGoldToUpgrade = getModderOption(MODDEROPTION_UPGRADE_MIN_GOLD);

		if (isModderOption(MODDEROPTION_UPGRADE_MOST_EXPENSIVE))
		{
			bool bUpgraded = true;

			// Keep looping while we have enough gold to upgrade, and we keep finding units to upgrade (cap at 2x num units to be safe)
			for (int i = 0; i < getNumUnits() * 2 && getGold() > iMinGoldToUpgrade && bUpgraded; ++i)
			{
				// Find unit with the highest cost upgrade available
				bst::optional<UnitUpgradeScore> bestUpgrade = algo::max_element(
					units() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade())
							| transformed(MostExpensiveUpgrade())
				);

				if (bestUpgrade && bestUpgrade->item)
				{
					CvUnit* unitToUpgrade = bestUpgrade->item->unit;
					const UnitTypes upgradeToApply = bestUpgrade->item->upgrade;
					bUpgraded = unitToUpgrade->upgrade(upgradeToApply);
					// Upgrade replaces the original unit with a new one, so old unit must be killed
					unitToUpgrade->doDelayedDeath();
				}
				else
				{
					bUpgraded = false;
				}

				FAssertMsg(i != getNumUnits() * 2 - 1, "Unit auto upgrade appears to have hit an infinite loop");
			}

		}
		else if (isModderOption(MODDEROPTION_UPGRADE_MOST_EXPERIENCED))
		{
			bool bUpgraded = true;

			// Keep looping while we have enough gold to upgrade, and we keep finding units to upgrade (cap at 2x num units to be safe)
			for (int i = 0; i < getNumUnits() * 2 && getGold() > iMinGoldToUpgrade && bUpgraded; ++i)
			{
				// Find unit with the highest available experience that has an upgrade available
				CvUnit* pBestUnit = nullptr;
				int iExperience = -1;
				foreach_(CvUnit * unit, units() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade()))
				{
					if (iExperience > unit->getExperience100())
						continue;

					foreach_(int iUpgrade, GC.getUnitInfo(unit->getUnitType()).getUnitUpgradeChain())
					{
						if (unit->canUpgrade((UnitTypes)iUpgrade))
						{
							iExperience = unit->getExperience100();
							pBestUnit = unit;
							break;
						}
					}
				}

				if (pBestUnit != nullptr)
				{
					// Use AI upgrade to choose the best upgrade
					bUpgraded = pBestUnit->AI_upgrade();
					// Upgrade replaces the original unit with a new one, so old unit must be killed
					pBestUnit->doDelayedDeath();
				}
				else
				{
					bUpgraded = false;
				}
				FAssertMsg(i != getNumUnits() * 2 - 1, "Unit auto upgrade appears to have hit an infinite loop");
			}
		}
		else
		{
			// Copy units as we will be removing and adding some now.
			foreach_(CvUnit * unit, units_safe() | filtered(CvUnit::fn::isAutoUpgrading() && CvUnit::fn::isReadyForUpgrade()))
			{
				unit->AI_upgrade();
				// Upgrade replaces the original unit with a new one, so old unit must be killed
				unit->doDelayedDeath();
			}
		}
		return;
	}

	const bool bAnyWar = GET_TEAM(getTeam()).hasWarPlan(true);
	const int64_t iStartingGold = getGold();
	const int64_t iTargetGold = AI_goldTarget();
	int64_t iUpgradeBudget = std::max<int64_t>(iStartingGold - iTargetGold, AI_goldToUpgradeAllUnits()) / (bAnyWar ? 1 : 2);

	iUpgradeBudget = std::min<int64_t>(iUpgradeBudget, (iStartingGold - iTargetGold < iUpgradeBudget) ? (iStartingGold - iTargetGold) : iStartingGold / 2);
	iUpgradeBudget = std::max<int64_t>(0, iUpgradeBudget);

	if (AI_isFinancialTrouble())
	{
		iUpgradeBudget /= 3;
	}

	if (gPlayerLogLevel > 2)
	{
		logBBAI("	%S calculates upgrade budget of %I64d from %I64d current gold, %I64d target", getCivilizationDescription(0), iUpgradeBudget, iStartingGold, iTargetGold);
	}

	// Always willing to upgrade 1 unit if we have the money
	iUpgradeBudget = std::max<int64_t>(iUpgradeBudget, 1);

	CvPlot* pLastUpgradePlot = NULL;
	for (int iPass = 0; iPass < 3; iPass++)
	{
		if (isNPC())
		{
			iPass = 3;
		}
		foreach_(CvUnit* unitX, units())
		{
			if (unitX->isDead() || !unitX->isReadyForUpgrade())
			{
				continue;
			}
			// Koshling - never upgrade workers or subdued animals here as they typically have outcome
			//	missions and construction capabilities that must be evaluated comparatively.
			//	The UnitAI processing for these AI types handles upgrade explicitly.
			switch (unitX->AI_getUnitAIType())
			{
				case UNITAI_SUBDUED_ANIMAL:
				case UNITAI_WORKER:
					continue;
				default: break;
			}
			CvPlot* unitPlot = unitX->plot();
			bool bNoDisband = getGold() > iTargetGold;
			bool bValid = false;

			switch (iPass)
			{
				case 0:
				{
					if (unitPlot->isCity())
					{
						if (unitPlot->getBestDefender(getID()) == unitX
						// try to upgrade units which are in danger... but don't get obsessed
						|| pLastUpgradePlot != unitPlot && AI_getAnyPlotDanger(unitPlot, 1, false))
						{
							bNoDisband = true;
							bValid = true;
							pLastUpgradePlot = unitPlot;
						}
					}
					break;
				}
				case 1:
				{
					// Unit types which are limited in what terrain they can operate.
					if (AI_unitImpassableCount(unitX->getUnitType()) > 0)
					{
						bValid = true;
					}
					else if (unitX->cargoSpace() > 0
					// Only normal transports
					&&  unitX->getSpecialCargo() == NO_SPECIALUNIT
					// Also upgrade escort ships
					||  unitX->AI_getUnitAIType() == UNITAI_ESCORT_SEA)
					{
						bValid = bAnyWar || iStartingGold - getGold() < iUpgradeBudget;
					}
					break;
				}
				case 2:
				{
					bValid = iStartingGold - getGold() < iUpgradeBudget;
					break;
				}
				case 3: // Special case for NPC
				{
					bValid = true;
					bNoDisband = true;
					break;
				}
				default:
				{
					FErrorMsg("error");
					break;
				}
			}
			// Kill off units
			if (!bNoDisband && unitX->canFight() && !unitX->isAnimal() && getUnitUpkeepNet(unitX->isMilitaryBranch(), unitX->getUpkeep100()) > 0)
			{
				CvCity* pPlotCity = unitPlot->getPlotCity();
				if (pPlotCity && pPlotCity->getOwner() == getID())
				{
					if ((unitX->getDomainType() != DOMAIN_LAND || unitPlot->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID()) > 1)
					&& unitPlot->getNumDefenders(getID()) > pPlotCity->AI_neededDefenders()
					&& pPlotCity->canTrain(unitX->getUnitType())
					&& (!unitX->canDefend() || !AI_getAnyPlotDanger(unitPlot, 2, false)))
					{
						int iCityExp = 0;
						if (unitX->getExperience() > 0)
						{
							iCityExp = (
								getFreeExperience() //civics & wonders
								+ pPlotCity->getFreeExperience()
								+ pPlotCity->getSpecialistFreeExperience()
								+ pPlotCity->getDomainFreeExperience(unitX->getDomainType())
								+ pPlotCity->getUnitCombatFreeExperience(unitX->getUnitCombatType())
							);
							foreach_(const UnitCombatTypes eSubCombat, unitX->getUnitInfo().getSubCombatTypes())
							{
								iCityExp += pPlotCity->getUnitCombatFreeExperience(eSubCombat);
							}
							// Afforess - also include wonder, religion & civic experience
							if (getStateReligion() != NO_RELIGION && pPlotCity->isHasReligion(getStateReligion()))
							{
								iCityExp += getStateReligionFreeExperience(); //religions
							}
							iCityExp = std::max(0, iCityExp);
						}
						if (unitX->getExperience() <= iCityExp)
						{
							if (unitX->hasCargo())
							{
								unitX->unloadAll();
							}
							unitX->getGroup()->AI_setMissionAI(MISSIONAI_DELIBERATE_KILL, NULL, NULL);
							unitX->kill(true);
							pLastUpgradePlot = NULL;
							continue;
						}
					}
				}
			}
			if (bValid)
			{
				unitX->AI_upgrade(); // CAN DELETE UNIT!!!
			}
		}
	}
	if (isNPC())
	{
		return;
	}
	if (gPlayerLogLevel > 2 && iStartingGold - getGold() > 0)
	{
		logBBAI(
			"	%S spends %I64d on unit upgrades out of budget of %I64d, %I64d effective gold remaining",
			getCivilizationDescription(0), iStartingGold - getGold(), iUpgradeBudget, getGold()
		);
	}
	AI_doSplit();
}


void CvPlayerAI::AI_doPeace()
{
	PROFILE_FUNC();
	FAssert(!isHumanPlayer());
	FAssert(!isMinorCiv());
	FAssert(!isNPC());

	CLinkList<TradeData> ourList;
	CLinkList<TradeData> theirList;
	bool abContacted[MAX_TEAMS];

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		abContacted[iI] = false;
	}

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive()
		&& iI != getID()
		&& canContact((PlayerTypes)iI)
		&& AI_isWillingToTalk((PlayerTypes)iI)
		&& !GET_TEAM(getTeam()).isHuman()
		&& (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || !GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHuman())
		&& GET_TEAM(getTeam()).isAtWar(GET_PLAYER((PlayerTypes)iI).getTeam())
		&& (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (GET_TEAM(getTeam()).getLeaderID() == getID()))
		&& AI_getContactTimer((PlayerTypes)iI, CONTACT_PEACE_TREATY) == 0)
		{
			FAssertMsg(!(GET_PLAYER((PlayerTypes)iI).isNPC()), "(GET_PLAYER((PlayerTypes)iI).isNPC()) did not return false as expected");
			FAssertMsg(iI != getID(), "iI is not expected to be equal with getID()");
			FAssert(GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam());

			{
				bool bConsiderPeace;
				if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
				{
					bConsiderPeace = (
							GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER((PlayerTypes)iI).getTeam()) > 10
						||	GET_TEAM(getTeam()).getAtWarCount(false, true) > 1
						||	GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_getWarSuccess(getTeam())
						>	GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER((PlayerTypes)iI).getTeam()) * 2
					);
				}
				else
				{
					bConsiderPeace = GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER((PlayerTypes)iI).getTeam()) > 10;
				}
				if (!bConsiderPeace)
				{
					continue;
				}
			}

			bool bOffered = false;

			TradeData item;
			setTradeItem(&item, TRADE_SURRENDER);

			if (canTradeItem((PlayerTypes)iI, item, true))
			{
				ourList.clear();
				theirList.clear();

				ourList.insertAtEnd(item);

				bOffered = true;

				if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
				{
					if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
					{
						AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_TREATY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_TREATY));
						CvDiploParameters* pDiplo = new CvDiploParameters(getID());
						FAssertMsg(pDiplo, "pDiplo must be valid");
						pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_PEACE"));
						pDiplo->setAIContact(true);
						pDiplo->setOurOfferList(theirList);
						pDiplo->setTheirOfferList(ourList);
						AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
						abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
					}
				}
				else if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_acceptSurrender(getTeam()))
				{
					GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
				}
			}

			if (!bOffered && GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PEACE_TREATY), "AI Diplo Peace Treaty") == 0)
			{
				setTradeItem(&item, TRADE_PEACE_TREATY);

				if (canTradeItem((PlayerTypes)iI, item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
				{
					int iOurValue = GET_TEAM(getTeam()).AI_endWarVal(GET_PLAYER((PlayerTypes)iI).getTeam());
					int iTheirValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_endWarVal(getTeam());

					TechTypes eBestReceiveTech = NO_TECH;
					TechTypes eBestGiveTech = NO_TECH;

					int iReceiveGold = 0;
					int iGiveGold = 0;

					CvCity* pBestReceiveCity = NULL;
					CvCity* pBestGiveCity = NULL;

					if (iTheirValue > iOurValue)
					{
						int iBestValue = 0;

						for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

							if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
							{
								const int iValue = (1 + GC.getGame().getSorenRandNum(10000, "AI Peace Trading (Tech #1)"));

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestReceiveTech = (TechTypes)iJ;
								}
							}
						}

						if (eBestReceiveTech != NO_TECH)
						{
							iOurValue += GET_TEAM(getTeam()).AI_techTradeVal(eBestReceiveTech, GET_PLAYER((PlayerTypes)iI).getTeam());
						}

						const int iGold = std::min((iTheirValue - iOurValue), GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()));

						if (iGold > 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGold);

							if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
							{
								iReceiveGold = iGold;
								iOurValue += iGold;
							}
						}

						if (iTheirValue > iOurValue)
						{
							iBestValue = 0;

							foreach_(CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
							{
								setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

								if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									const int iValue = pLoopCity->plot()->calculateCulturePercent(getID());

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestReceiveCity = pLoopCity;
									}
								}
							}

							if (pBestReceiveCity)
							{
								iOurValue += AI_cityTradeVal(pBestReceiveCity);
							}
						}
					}
					else if (iOurValue > iTheirValue)
					{
						int iBestValue = 0;

						for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal((TechTypes)iJ, getTeam()) <= (iOurValue - iTheirValue))
								{
									const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Peace Trading (Tech #2)");

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestGiveTech = (TechTypes)iJ;
									}
								}
							}
						}

						if (eBestGiveTech != NO_TECH)
						{
							iTheirValue += GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam());
						}

						const int iGold = std::min((iOurValue - iTheirValue), AI_maxGoldTrade((PlayerTypes)iI));

						if (iGold > 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGold);

							if (canTradeItem((PlayerTypes)iI, item, true))
							{
								iGiveGold = iGold;
								iTheirValue += iGold;
							}
						}

						iBestValue = 0;

						foreach_(CvCity * pLoopCity, cities())
						{
							setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								if (GET_PLAYER((PlayerTypes)iI).AI_cityTradeVal(pLoopCity) <= (iOurValue - iTheirValue))
								{
									const int iValue = pLoopCity->plot()->calculateCulturePercent((PlayerTypes)iI);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestGiveCity = pLoopCity;
									}
								}
							}
						}

						if (pBestGiveCity)
						{
							iTheirValue += GET_PLAYER((PlayerTypes)iI).AI_cityTradeVal(pBestGiveCity);
						}
					}

					if ((GET_PLAYER((PlayerTypes)iI).isHumanPlayer()) ? (iOurValue >= iTheirValue) : ((iOurValue > ((iTheirValue * 3) / 5)) && (iTheirValue > ((iOurValue * 3) / 5))))
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_PEACE_TREATY);

						ourList.insertAtEnd(item);
						theirList.insertAtEnd(item);

						if (eBestGiveTech != NO_TECH)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
							ourList.insertAtEnd(item);
						}

						if (eBestReceiveTech != NO_TECH)
						{
							setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
							theirList.insertAtEnd(item);
						}

						if (iGiveGold != 0)
						{
							setTradeItem(&item, TRADE_GOLD, iGiveGold);
							ourList.insertAtEnd(item);
						}

						if (iReceiveGold != 0)
						{
							setTradeItem(&item, TRADE_GOLD, iReceiveGold);
							theirList.insertAtEnd(item);
						}

						if (pBestGiveCity)
						{
							setTradeItem(&item, TRADE_CITIES, pBestGiveCity->getID());
							ourList.insertAtEnd(item);
						}

						if (pBestReceiveCity)
						{
							setTradeItem(&item, TRADE_CITIES, pBestReceiveCity->getID());
							theirList.insertAtEnd(item);
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_TREATY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_TREATY));
								CvDiploParameters* pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_PEACE"));
								pDiplo->setAIContact(true);
								pDiplo->setOurOfferList(theirList);
								pDiplo->setTheirOfferList(ourList);
								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else
						{
							GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
						}
					}
				}
			}
		}
	}
}

void CvPlayerAI::AI_updateFoundValues(bool bClear, const CvArea* area) const
{
	PROFILE_FUNC();

	const bool bSetup = getNumCities() == 0;

	if (bClear)
	{
		m_bCitySitesNotCalculated = true;

		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* plotX = GC.getMap().plotByIndex(iI);

			if (bSetup || plotX->isRevealed(getTeam(), false))
			{
				GC.getMap().plotByIndex(iI)->clearFoundValue(getID());
			}
		}
		algo::for_each(GC.getMap().areas(), CvArea::fn::setBestFoundValue(getID(), -1));
		return;
	}
	std::vector<CvArea*> aUncalculatedAreas;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		CvPlot* plotX = GC.getMap().plotByIndex(iI);
		CvArea* areaX = plotX->area();

		if ((area == NULL || areaX == area) && (bSetup || plotX->isRevealed(getTeam(), false)))
		{
			bool bNeedsCalculating = false;

			if (!areaX->hasBestFoundValue(getID()))
			{
				bNeedsCalculating = true;
				areaX->setBestFoundValue(getID(), 0);
				aUncalculatedAreas.push_back(areaX);
			}
			else if (algo::any_of_equal(aUncalculatedAreas, areaX))
			{
				bNeedsCalculating = true;
			}

			if (bNeedsCalculating)
			{
				const int iValue = AI_foundValue(plotX->getX(), plotX->getY());

				plotX->setFoundValue(getID(), iValue);

				if (iValue > areaX->getBestFoundValue(getID()))
				{
					areaX->setBestFoundValue(getID(), iValue);
				}
			}
		}
	}
}


void CvPlayerAI::AI_updateAreaTargets()
{
	PROFILE_EXTRA_FUNC();
	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		if (!pLoopArea->isWater())
		{
			if (GC.getGame().getSorenRandNum(3, "AI Target City") == 0)
			{
				pLoopArea->setTargetCity(getID(), NULL);
			}
			else pLoopArea->setTargetCity(getID(), AI_findTargetCity(pLoopArea));
		}
	}
}


// Returns priority for unit movement (lower values move first...)
int CvPlayerAI::AI_movementPriority(const CvSelectionGroup* pGroup) const
{
	const CvUnit* pHeadUnit = pGroup->getHeadUnit();

	if (pHeadUnit == NULL)
	{
		return 100; // Lowest priority
	}
	if (pHeadUnit->isSpy())
	{
		return 0; // Highest priority
	}

	if (pHeadUnit->hasCargo())
	{
		// General transport before specialized
		return pHeadUnit->getSpecialCargo() == NO_SPECIALUNIT ? 1 : 2;
	}

	if (pHeadUnit->getDomainType() == DOMAIN_AIR)
	{
		// Fighters before bombers, they are better at clearing out air defenses
		return pHeadUnit->canAirDefend() ? 3 : 4;
	}

	if (pHeadUnit->AI_getUnitAIType() == UNITAI_WORKER || pHeadUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA)
	{
		return 5;
	}

	if (pHeadUnit->AI_getUnitAIType() == UNITAI_EXPLORE || pHeadUnit->AI_getUnitAIType() == UNITAI_EXPLORE_SEA)
	{
		return 6;
	}

	if (pHeadUnit->getBombardRate() > 0)
	{
		return 7;
	}

	if (pHeadUnit->collateralDamage() > 0)
	{
		return 8;
	}

	if (pHeadUnit->canFight())
	{
		// Skirmishers should harass before regular fighters engage
		const int iWithdraw = pHeadUnit->withdrawalProbability();
		if (iWithdraw > 49)
		{
			return 9;
		}
		if (iWithdraw > 39)
		{
			return 10;
		}
		if (iWithdraw > 29)
		{
			return 11;
		}
		if (iWithdraw > 19)
		{
			return 12;
		}
		if (iWithdraw > 9)
		{
			return 13;
		}

		int iCurrCombat = pHeadUnit->currCombatStr(NULL, NULL);

		if (pHeadUnit->noDefensiveBonus())
		{
			// Move poor defenders first
			iCurrCombat *= 3;
			iCurrCombat /= 2;
		}

		if (pHeadUnit->AI_isCityAIType())
		{
			iCurrCombat /= 2;
		}
		const int iBestCombat = 100 * GC.getGame().getBestLandUnitCombat();

		if (iCurrCombat > iBestCombat)
		{
			return 20;
		}
		if (iCurrCombat > iBestCombat * 4 / 5)
		{
			return 25;
		}
		if (iCurrCombat > iBestCombat * 3 / 5)
		{
			return 30;
		}
		if (iCurrCombat > iBestCombat * 2 / 5)
		{
			return 35;
		}
		if (iCurrCombat > iBestCombat / 5)
		{
			return 40;
		}
		return 45;
	}
	return 50;
}


void CvPlayerAI::AI_unitUpdate()
{
	PROFILE_FUNC();

	//	Do delayed death here so that empty groups are removed now
	foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		pLoopSelectionGroup->doDelayedDeath(); // could destroy the selection group...
	}

	//	Should have the same set of groups as is represented by m_selectionGroups
	//	as (indirectly via their head units) by m_groupCycles.  These have been seen to
	//	get out of step, which results in a WFoC so if they contain differing member
	//	counts go through and fix it!
	//	Note - this is fixing a symptom rather than a cause which is distasteful, but as
	//	yet the cause remains elusive
	if (m_groupCycles[CURRENT_MAP]->getLength() != m_selectionGroups[CURRENT_MAP]->getCount() - (m_pTempUnit ? 1 : 0))
	{
		/*
		if (m_pTempUnit)
		{
			FAssert(m_pTempUnit->getGroup() != NULL);
			OutputDebugString(CvString::format("temp group id is %d\n", m_pTempUnit->getGroup()->getID()).c_str());
		}
		*/
		OutputDebugString("Group cycle:\n");
		for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL; pCurrUnitNode = nextGroupCycleNode(pCurrUnitNode))
		{
			CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
			OutputDebugString(CvString::format("	%d with %d units at (%d,%d)\n",
				pLoopSelectionGroup->getID(),
				pLoopSelectionGroup->getNumUnits(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getX(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getY()).c_str());
		}

		FAssert(m_selectionGroups[CURRENT_MAP]->getCount() > m_groupCycles[CURRENT_MAP]->getLength());	//	Other way round not seen - not handled currently
		OutputDebugString("Selection groups:\n");
		foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
		{
			OutputDebugString(CvString::format("	%d with %d units at (%d,%d)\n",
				pLoopSelectionGroup->getID(),
				pLoopSelectionGroup->getNumUnits(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getX(),
				pLoopSelectionGroup->plot() == NULL ? -1 : pLoopSelectionGroup->getY()).c_str());
			if (pLoopSelectionGroup->getHeadUnit() != m_pTempUnit)
			{
				updateGroupCycle(pLoopSelectionGroup->getHeadUnit(), true);
			}
		}
	}

	if (!hasBusyUnit())
	{
		for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL;)
		{
			CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
			CLLNode<int>* pNextUnitNode = nextGroupCycleNode(pCurrUnitNode);

			//	Since we know the set of selection groups can (somehow - reason is unknown) get out of step
			//	with the set in the update cycle (hence the code in the section above this one that copes with
			//	selection groups that don't have group cycle entries), it follows that the converse may also
			//	occur.  Thus we must be prepared to handle cycle entries with no corresponding selection group
			//	still extant
			if (pLoopSelectionGroup == NULL)
			{
				deleteGroupCycleNode(pCurrUnitNode);
			}
			else if (pLoopSelectionGroup->AI_isForceSeparate())
			{
				// do not split groups that are in the midst of attacking
				if (pLoopSelectionGroup->isForceUpdate() || !pLoopSelectionGroup->AI_isGroupAttack())
				{
					pLoopSelectionGroup->AI_separate();	// pointers could become invalid...
				}
			}

			pCurrUnitNode = pNextUnitNode;
		}

		if (isHumanPlayer())
		{
			for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL;)
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
				CLLNode<int>* pNextUnitNode = nextGroupCycleNode(pCurrUnitNode);

				if (pLoopSelectionGroup == NULL)
				{
					deleteGroupCycleNode(pCurrUnitNode);
				}
				else if (pLoopSelectionGroup->AI_update())
				{
					break; // pointers could become invalid...
				}
				pCurrUnitNode = pNextUnitNode;
			}
		}
		else
		{
			std::vector< std::pair<int, int> > groupList;

			for (CLLNode<int>* pCurrUnitNode = headGroupCycleNode(); pCurrUnitNode != NULL; pCurrUnitNode = nextGroupCycleNode(pCurrUnitNode))
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(pCurrUnitNode->m_data);
				FAssert(pLoopSelectionGroup != NULL);

				int iPriority = AI_movementPriority(pLoopSelectionGroup);
				groupList.push_back(std::make_pair(iPriority, pCurrUnitNode->m_data));
			}

			algo::sort(groupList);
			for (size_t i = 0; i < groupList.size(); i++)
			{
				CvSelectionGroup* pLoopSelectionGroup = getSelectionGroup(groupList[i].second);

				if (pLoopSelectionGroup && pLoopSelectionGroup->AI_update())
				{
					FAssert(pLoopSelectionGroup && pLoopSelectionGroup->getNumUnits() > 0);
					break;
				}
			}
		}
	}
}


void CvPlayerAI::AI_makeAssignWorkDirty()
{
	algo::for_each(cities(), CvCity::fn::AI_setAssignWorkDirty(true));
}


void CvPlayerAI::AI_assignWorkingPlots()
{
	if (isAnarchy())
	{
		return; // No point
	}
	algo::for_each(cities(), CvCity::fn::AI_assignWorkingPlots());
}


void CvPlayerAI::AI_updateAssignWork()
{
	algo::for_each(cities(), CvCity::fn::AI_updateAssignWork());
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  05/08/09								jdog5000	  */
/*																							  */
/* City AI																					  */
/************************************************************************************************/
void CvPlayerAI::AI_doCentralizedProduction()
{
	PROFILE_FUNC();

	if (isHumanPlayer() || isNPC())
	{
		return;
	}

	// BBAI TODO: Temp testing
	//if( getID() % 2 == 1 )
	//{
	return;
	//}

//#if 0 // Disabled to stop static analysis from triggering [11/12/2019 billw]
//	CvCity* pLoopCity;
//	int iLoop;
//	int iI;
//	// Determine number of cities player can use building wonders currently
//	int iMaxNumWonderCities = 1 + getNumCities() / 5;
//	bool bIndustrious = (getMaxPlayerBuildingProductionModifier() > 0);
//	bool bAtWar = GET_TEAM(getTeam()).isAtWar();
//
//	if (bIndustrious)
//	{
//		iMaxNumWonderCities += 1;
//	}
//
//	// Danger?
//	// Power?
//	// Research?
//
//	if (bAtWar)
//	{
//		int iWarCapRatio = GET_TEAM(getTeam()).AI_getWarSuccessCapitulationRatio();
//		if (iWarCapRatio < -90)
//		{
//			iMaxNumWonderCities = 0;
//		}
//		else
//		{
//			if (iWarCapRatio < 30)
//			{
//				iMaxNumWonderCities -= 1;
//			}
//			if (iWarCapRatio < -50)
//			{
//				iMaxNumWonderCities /= 2;
//			}
//		}
//	}
//
//	if (isMinorCiv() && (GET_TEAM(getTeam()).getHasMetCivCount(false) > 1))
//	{
//		iMaxNumWonderCities /= 2;
//	}
//
//	iMaxNumWonderCities = std::min(iMaxNumWonderCities, getNumCities());
//
//	// Gather city statistics
//	// Could rank cities based on gold, production here, could be O(n) instead of O(n^2)
//	int iWorldWonderCities = 0;
//	int iLimitedWonderCities = 0;
//	int iNumDangerCities = 0;
//	foreach_(const CvCity * pLoopCity, cities())
//	{
//		if (pLoopCity->isProductionBuilding())
//		{
//			if (isLimitedWonder(pLoopCity->getProductionBuilding()))
//			{
//				iLimitedWonderCities++;
//
//				if (isWorldWonder(pLoopCity->getProductionBuilding()))
//				{
//					iWorldWonderCities++;
//				}
//			}
//		}
//
//		if (pLoopCity->isProductionProject())
//		{
//			if (isLimitedProject(pLoopCity->getProductionProject()))
//			{
//				iLimitedWonderCities++;
//				if (isWorldProject(pLoopCity->getProductionProject()))
//				{
//					iWorldWonderCities++;
//				}
//			}
//		}
//	}
//
//	// Check for any projects to build
//	for (iI = 0; iI < GC.getNumProjectInfos(); iI++)
//	{
//
//	}
//
//	// Check for any national/team wonders to build
//#endif
}
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/


void CvPlayerAI::AI_makeProductionDirty()
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	algo::for_each(cities(), CvCity::fn::AI_setChooseProductionDirty(true));
}

// War tactics AI
void CvPlayerAI::AI_conquerCity(PlayerTypes eOldOwner, CvCity* pCity, bool bConquest, bool bTrade)
{
	PROFILE_EXTRA_FUNC();
	bool bRaze = false;

	if ( // Can raze
			!GC.getGame().isOption(GAMEOPTION_NO_CITY_RAZING)
		&& ( // Can't raze if same ID ever owned, or if random chance. Can raze max culture level cities.
			!pCity->isEverOwned(getID())
			|| pCity->calculateTeamCulturePercent(getTeam()) < GC.getDefineINT("RAZING_CULTURAL_PERCENT_THRESHOLD")
			)
	)
	{
		int iRazeValue = 0;
		const int iCloseness = pCity->AI_playerCloseness(getID());

		// Reasons to always raze
		if (GC.getGame().culturalVictoryValid() && GET_TEAM(getTeam()).AI_getEnemyPowerPercent(false) > 75)
		{
			const int iHighCultureThreshold = GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel()) * 3/5;

			if (pCity->getCulture(eOldOwner) > iHighCultureThreshold)
			{
				int iHighCultureCount = 1;

				foreach_(const CvCity * cityX, GET_PLAYER(eOldOwner).cities())
				{
					if (cityX->getCulture(eOldOwner) > iHighCultureThreshold)
					{
						iHighCultureCount++;
						if (iHighCultureCount >= GC.getGame().culturalVictoryNumCultureCities())
						{
							//Raze city enemy needs for cultural victory unless we greatly over power them
							OutputDebugString("  Razing enemy cultural victory city");
							bRaze = true;
						}
					}
				}
			}
		}

		if (!bRaze)
		{
			// Reasons to not raze
			if (getNumCities() <= 1 || getNumCities() < 5 && iCloseness > 0)
			{
				if (gPlayerLogLevel > 0)
				{
					logBBAI("	Player %d (%S) decides not to raze %S because they have few cities", getID(), getCivilizationDescription(0), pCity->getName().GetCString());
				}
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3) && GET_TEAM(getTeam()).AI_isPrimaryArea(pCity->area()))
			{
				// Do not raze, going for domination
				if (gPlayerLogLevel > 0)
				{
					logBBAI("	Player %d (%S) decides not to raze %S because they're going for domination", getID(), getCivilizationDescription(0), pCity->getName().GetCString());
				}
			}
			else if (isHominid())
			{
				if (!pCity->isHolyCity() && !pCity->hasActiveWorldWonder()
				&& eOldOwner != BARBARIAN_PLAYER && eOldOwner != NEANDERTHAL_PLAYER
				&& pCity->getOriginalOwner() != BARBARIAN_PLAYER && pCity->getOriginalOwner() != NEANDERTHAL_PLAYER)
				{
					iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
					iRazeValue -= iCloseness;
				}
			}
			else
			{
				const bool bFinancialTrouble = AI_isFinancialTrouble();
				const bool bPrevOwnerBarb = (eOldOwner == BARBARIAN_PLAYER || eOldOwner == NEANDERTHAL_PLAYER);
				const bool bBarbCity = bPrevOwnerBarb && (pCity->getOriginalOwner() == BARBARIAN_PLAYER || pCity->getOriginalOwner() == NEANDERTHAL_PLAYER);

				if (GET_TEAM(getTeam()).countNumCitiesByArea(pCity->area()) == 0)
				{
					// Conquered city in new continent/island
					int iBestValue;
					if (pCity->area()->getNumCities() == 1 && AI_getNumAreaCitySites(pCity->area()->getID(), iBestValue) == 0)
					{
						if (iCloseness == 0) // Probably small island
						{
							// Safe to raze these now that AI can do pick up ...
							iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
						}
					}
					else if (iCloseness < 10) // At least medium sized island
					{
						if (bFinancialTrouble)
						{// Raze if we might start incuring colony maintenance
							iRazeValue = 100;
						}
						else if (eOldOwner != NO_PLAYER && !bPrevOwnerBarb
						&& GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).countNumCitiesByArea(pCity->area()) > 3)
						{
							if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
							{
								iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
							}
							else
							{
								iRazeValue += std::max(0, (GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb() - iCloseness));
							}
						}
					}
				}
				else
				{
					// Distance related aspects
					if (iCloseness > 0)
					{
						iRazeValue -= iCloseness;
					}
					else
					{
						iRazeValue += 40;

						CvCity* pNearestTeamAreaCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), NO_PLAYER, getTeam(), true, false, NO_TEAM, NO_DIRECTION, pCity);

						if (pNearestTeamAreaCity == NULL)
						{
							// Shouldn't happen
							iRazeValue += 30;
						}
						else
						{
							const int iDistance = plotDistance(pCity->getX(), pCity->getY(), pNearestTeamAreaCity->getX(), pNearestTeamAreaCity->getY()) - DEFAULT_PLAYER_CLOSENESS - 2;
							if (iDistance > 0)
							{
								iRazeValue += iDistance * (bBarbCity ? 8 : 5);
							}
						}
					}

					if (bFinancialTrouble)
					{
						iRazeValue += std::max(0, (70 - 15 * pCity->getPopulation()));
					}

					// Scale down distance/maintenance effects for organized
					if (iRazeValue > 0)
					{
						for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
						{
							if (hasTrait((TraitTypes)iI))
							{
								iRazeValue *= (100 - (GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier()));
								iRazeValue /= 100;

								if (gPlayerLogLevel > 0 && GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier() > 0)
								{
									logBBAI("	  Reduction for upkeep modifier %d", GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier());
								}
							}
						}
					}

					// Non-distance related aspects
					if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
					{
						iRazeValue += GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb();
					}
					else
					{
						iRazeValue += std::max(0, ((GC.getLeaderHeadInfo(getPersonalityType()).getRazeCityProb() / 2) - iCloseness));
					}
					if (getStateReligion() != NO_RELIGION && pCity->isHasReligion(getStateReligion()))
					{
						if (GET_TEAM(getTeam()).hasShrine(getStateReligion()))
						{
							iRazeValue -= 50;

							if (gPlayerLogLevel > 0)
							{
								logBBAI("	  Reduction for state religion with shrine");
							}
						}
						else
						{
							iRazeValue -= 10;

							if (gPlayerLogLevel > 0)
							{
								logBBAI("	  Reduction for state religion");
							}
						}
					}
				}


				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (pCity->isHolyCity((ReligionTypes)iI))
					{
						OutputDebugString("	  Reduction for holy city");

						if (getStateReligion() == iI)
						{
							iRazeValue -= 150;
						}
						else
						{
							iRazeValue -= 5 + GC.getGame().calculateReligionPercent((ReligionTypes)iI);
						}
					}
				}

				iRazeValue -= 25 * pCity->getNumActiveWorldWonders();

				iRazeValue -= pCity->calculateTeamCulturePercent(getTeam());

				for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
				{
					const CvPlot* pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);

					if (pLoopPlot != NULL && pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
					{
						iRazeValue -= std::max(2, AI_bonusVal(pLoopPlot->getBonusType(getTeam())) / 2);
					}
				}

				// More inclined to raze if we're unlikely to hold it
				if (GET_TEAM(getTeam()).getPower(false) * 10 < GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).getPower(true) * 8)
				{
					const int iTempValue = std::min(75,
						(GET_TEAM(GET_PLAYER(eOldOwner).getTeam()).getPower(true) - GET_TEAM(getTeam()).getPower(false))
						* 20 / std::max(100, GET_TEAM(getTeam()).getPower(false))
					);
					logBBAI("	  Low power, so boost raze odds by %d", iTempValue);
					iRazeValue += iTempValue;
				}

				if (gPlayerLogLevel >= 1)
				{
					if (bBarbCity) logBBAI("	  %S is a barb city", pCity->getName().GetCString());
					if (bPrevOwnerBarb) logBBAI("	  %S was last owned by barbs", pCity->getName().GetCString());
					logBBAI("	  %S has area cities %d, closeness %d, bFinTrouble %d", pCity->getName().GetCString(), GET_TEAM(getTeam()).countNumCitiesByArea(pCity->area()), iCloseness, bFinancialTrouble);
				}
			}

			if (gPlayerLogLevel >= 1)
			{
				logBBAI("	Player %d (%S) has odds %d to raze city %S", getID(), getCivilizationDescription(0), iRazeValue, pCity->getName().GetCString());
			}

			if (iRazeValue > 0 && GC.getGame().getSorenRandNum(100, "AI Raze City") < iRazeValue)
			{
				bRaze = true;
			}
		}
	}

	if (bRaze)
	{
		logBBAI("	Player %d (%S) decides to to raze city %S!!!", getID(), getCivilizationDescription(0), pCity->getName().GetCString());
		pCity->doTask(TASK_RAZE);
	}
	else
	{
		CvEventReporter::getInstance().cityAcquiredAndKept(eOldOwner, getID(), pCity, bConquest, bTrade);
	}
}


bool CvPlayerAI::AI_acceptUnit(const CvUnit* pUnit) const
{
	if (isHumanPlayer())
	{
		return true;
	}

	if (AI_isFinancialTrouble())
	{
		if (pUnit->AI_getUnitAIType() == UNITAI_WORKER)
		{
			if (AI_neededWorkers(pUnit->area()) > 0)
			{
				return true;
			}
		}

		if (pUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA)
		{
			return true;
		}

		if (pUnit->AI_getUnitAIType() == UNITAI_MISSIONARY)
		{
			return true; //XXX
		}
		return false;
	}

	return true;
}


DomainTypes CvPlayerAI::AI_unitAIDomainType(UnitAITypes eUnitAI) const
{
	switch (eUnitAI)
	{
	case UNITAI_UNKNOWN:
		return NO_DOMAIN;
		break;

	case UNITAI_ANIMAL:
	case UNITAI_SETTLE:
	case UNITAI_WORKER:
	case UNITAI_ATTACK:
	case UNITAI_ATTACK_CITY:
	case UNITAI_COLLATERAL:
	case UNITAI_PILLAGE:
	case UNITAI_RESERVE:
	case UNITAI_COUNTER:
	case UNITAI_PARADROP:
	case UNITAI_CITY_DEFENSE:
	case UNITAI_CITY_COUNTER:
	case UNITAI_CITY_SPECIAL:
	case UNITAI_EXPLORE:
	case UNITAI_MISSIONARY:
	case UNITAI_PROPHET:
	case UNITAI_ARTIST:
	case UNITAI_SCIENTIST:
	case UNITAI_GENERAL:
	case UNITAI_GREAT_HUNTER:
	case UNITAI_GREAT_ADMIRAL:
	case UNITAI_MERCHANT:
	case UNITAI_ENGINEER:
	case UNITAI_SPY:
	case UNITAI_ATTACK_CITY_LEMMING:
	case UNITAI_PILLAGE_COUNTER:
	case UNITAI_SUBDUED_ANIMAL:
	case UNITAI_HUNTER:
	case UNITAI_HUNTER_ESCORT:
	case UNITAI_HEALER:
	case UNITAI_PROPERTY_CONTROL:
	case UNITAI_BARB_CRIMINAL:
	case UNITAI_INVESTIGATOR:
	case UNITAI_INFILTRATOR:
	case UNITAI_SEE_INVISIBLE:
	case UNITAI_ESCORT:
		return DOMAIN_LAND;
		break;

	case UNITAI_ICBM:
		return DOMAIN_IMMOBILE;
		break;

	case UNITAI_WORKER_SEA:
	case UNITAI_ATTACK_SEA:
	case UNITAI_RESERVE_SEA:
	case UNITAI_ESCORT_SEA:
	case UNITAI_EXPLORE_SEA:
	case UNITAI_ASSAULT_SEA:
	case UNITAI_SETTLER_SEA:
	case UNITAI_MISSIONARY_SEA:
	case UNITAI_SPY_SEA:
	case UNITAI_CARRIER_SEA:
	case UNITAI_MISSILE_CARRIER_SEA:
	case UNITAI_PIRATE_SEA:
	case UNITAI_HEALER_SEA:
	case UNITAI_PROPERTY_CONTROL_SEA:
	case UNITAI_SEE_INVISIBLE_SEA:
		return DOMAIN_SEA;
		break;

	case UNITAI_ATTACK_AIR:
	case UNITAI_DEFENSE_AIR:
	case UNITAI_CARRIER_AIR:
	case UNITAI_MISSILE_AIR:
		return DOMAIN_AIR;
		break;

	default:
		FErrorMsg("error");
		break;
	}

	return NO_DOMAIN;
}


int CvPlayerAI::AI_yieldWeight(YieldTypes eYield) const
{
	if (eYield == YIELD_PRODUCTION)
	{
		const int iMod = 100 + 30 * std::max(0, GC.getGame().getCurrentEra() - 1) / std::max(1, GC.getNumEraInfos() - 2);
		return GC.getYieldInfo(eYield).getAIWeightPercent() * iMod / 100;
	}
	return GC.getYieldInfo(eYield).getAIWeightPercent();
}


int CvPlayerAI::AI_commerceWeight(CommerceTypes eCommerce, const CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	int iWeight = GC.getCommerceInfo(eCommerce).getAIWeightPercent();

	switch (eCommerce)
	{
	case COMMERCE_RESEARCH:
	{
		if (AI_avoidScience())
		{
			if (isNoResearchAvailable())
			{
				iWeight = 0;
			}
			else
			{
				iWeight /= 8;
			}
		}
		else if (!AI_isFinancialTrouble())
		{
			iWeight += 25;
		}
		break;
	}
	case COMMERCE_GOLD:
	{
		if (AI_isFinancialTrouble())
		{
			iWeight *= 2;
		}
		else if (getCommercePercent(COMMERCE_GOLD) < 25) // Low tax
		{
			//put more money towards other commerce types
			if (getGoldPerTurn() > getGold() / 40)
			{
				iWeight -= 50 - 2 * getCommercePercent(COMMERCE_GOLD);
			}
		}
		break;
	}
	case COMMERCE_CULTURE:
	{
		// COMMERCE_CULTURE AIWeightPercent is 25% in default xml
		// Adjustments for human player going for cultural victory (who won't have AI strategy set)
		// so that governors do smart things
		if (pCity != NULL)
		{
			if (GC.getGame().culturalVictoryValid() && pCity->getCultureTimes100(getID()) >= 100 * GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel()))
			{
				iWeight /= 50;
			}
			// Slider check works for detection of whether human player is going for cultural victory
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) || getCommercePercent(COMMERCE_CULTURE) >= 90)
			{
				int iCultureRateRank = pCity->findCommerceRateRank(COMMERCE_CULTURE);
				int iCulturalVictoryNumCultureCities = GC.getGame().culturalVictoryNumCultureCities();

				// if one of the currently best cities, then focus hard, *4 or more
				if (iCultureRateRank <= iCulturalVictoryNumCultureCities)
				{
					iWeight *= (3 + iCultureRateRank);
				}
				// if one of the 3 close to the top, then still emphasize culture some, *2
				else if (iCultureRateRank <= iCulturalVictoryNumCultureCities + 3)
				{
					iWeight *= 2;
				}
				else if (isHumanPlayer())
				{
					iWeight *= 2;
				}
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) || getCommercePercent(COMMERCE_CULTURE) >= 70)
			{
				iWeight *= 3;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1) || getCommercePercent(COMMERCE_CULTURE) >= 50)
			{
				iWeight *= 2;
			}
			iWeight += 2 * (100 - pCity->plot()->calculateCulturePercent(getID()));

			if (pCity->getCultureLevel() < (CultureLevelTypes)2)
			{
				iWeight = std::max(iWeight, 800);
			}
		}
		else // pCity == NULL
		{
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) || getCommercePercent(COMMERCE_CULTURE) >= 90)
			{
				iWeight *= 3;
				iWeight /= 4;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) || getCommercePercent(COMMERCE_CULTURE) >= 70)
			{
				iWeight *= 2;
				iWeight /= 3;
			}
			else if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1) || getCommercePercent(COMMERCE_CULTURE) >= 50)
			{
				iWeight /= 2;
			}
			else
			{
				iWeight /= 3;
			}
		}
		break;
	}
	case COMMERCE_ESPIONAGE:
	{
		const TeamTypes eTeam = getTeam();
		const CvTeamAI& myTeam = GET_TEAM(eTeam);

		if (!myTeam.hasMetAnyCiv())
		{
			iWeight = 0;
			break;
		}
		int iNumValidTeamsMet = 0;
		int iEspBehindWeight = 0;
		for (int iI = 0; iI < MAX_PC_TEAMS; ++iI)
		{
			const TeamTypes eTeamX = static_cast<TeamTypes>(iI);
			CvTeam& teamX = GET_TEAM(eTeamX);

			if (teamX.isAlive() && eTeamX != eTeam && myTeam.isHasMet(eTeamX)
			// Don't bother with minor civs unless we are one too.
			&& (isMinorCiv() || !teamX.isMinorCiv())
			// Ignore vassals
			&& !teamX.isVassal(eTeam) && !myTeam.isVassal(eTeamX))
			{
				iNumValidTeamsMet++;
				// Behind in espionage
				if (teamX.getEspionagePointsAgainstTeam(eTeam) - myTeam.getEspionagePointsAgainstTeam(eTeamX) > 0)
				{
					iEspBehindWeight++;

					if (myTeam.AI_getAttitude(eTeamX) < ATTITUDE_CAUTIOUS)
					{
						iEspBehindWeight++;
					}
				}
			}
		}
		if (iNumValidTeamsMet == 0)
		{
			iWeight = 0;
			break;
		}
		iWeight *= AI_getEspionageWeight() * (3 * iEspBehindWeight + iNumValidTeamsMet / 2 + 1);
		iWeight /= 200 * iNumValidTeamsMet;

		// K-Mod
		if (AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
		{
			iWeight *= 2;
		}

		if (getCommercePercent(COMMERCE_ESPIONAGE) == 0)
		{
			iWeight *= 2;
			iWeight /= 3;
		}
		else if (isHumanPlayer())
		{
			// UNOFFICIAL_PATCH todo:  should this tweak come over in some form?
			// There's still an issue with upping espionage slider for human player.
			if (getCommercePercent(COMMERCE_ESPIONAGE) > 50)
			{
				iWeight *= getCommercePercent(COMMERCE_ESPIONAGE);
				iWeight /= 50;
			}
		}
		// AI Espionage slider use maxed out at 20 percent
		else if (getCommercePercent(COMMERCE_ESPIONAGE) >= 20)
		{
			iWeight *= 3;
			iWeight /= 2;
		}
		break;
	}
	default: break;
	}
	return iWeight;
}

// Improved as per Blake - thanks!
int CvPlayerAI::AI_foundValue(int iX, int iY, int iMinRivalRange, bool bStartingLoc) const
{
	PROFILE_EXTRA_FUNC();
	if (!canFound(iX, iY))
	{
		return 0;
	}
	CvPlot* pPlot = GC.getMap().plot(iX, iY);

	bool bAdvancedStart = getAdvancedStartPoints() >= 0;
	const bool bIsCoastal = pPlot->isCoastalLand(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize());

	CvArea* pArea = pPlot->area();
	const int iNumAreaCities = pArea->getCitiesPerPlayer(getID());

	if (!bStartingLoc && !bAdvancedStart && !bIsCoastal && iNumAreaCities == 0)
	{
		return 0;
	}

	if (bAdvancedStart)
	{
		FAssert(GC.getGame().isOption(GAMEOPTION_CORE_CUSTOM_START));
		if (bStartingLoc)
		{
			bAdvancedStart = false;
		}
	}

	/* Explanation of city site adjustment:
		Any plot which is otherwise within the radius of a city site
		is basically treated as if it's within an existing city radius
	*/
	std::vector<bool> abCitySiteRadius(NUM_CITY_PLOTS, false);

	if (!bStartingLoc && !m_bCitySitesNotCalculated && !AI_isPlotCitySite(pPlot))
	{
		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);
			if (pLoopPlot == NULL)
			{
				continue;
			}
			for (int iJ = 0; iJ < AI_getNumCitySites(); iJ++)
			{
				const CvPlot* pCitySitePlot = AI_getCitySite(iJ);
				if (pCitySitePlot != pPlot && plotDistance(pLoopPlot->getX(), pLoopPlot->getY(), pCitySitePlot->getX(), pCitySitePlot->getY()) <= CITY_PLOTS_RADIUS)
				{ // Plot is inside the radius of a city site
					abCitySiteRadius[iI] = true;
					break;
				}
			}
		}
	}

	std::vector<int> paiBonusCount(GC.getNumBonusInfos(), 0);

	if (iMinRivalRange != -1)
	{
		foreach_(const CvPlot * pLoopPlot, pPlot->rect(iMinRivalRange, iMinRivalRange))
		{
			if (pLoopPlot->plotCheck(PUF_isOtherTeam, getID()) != NULL)
			{
				return 0;
			}
		}
	}

	if (bStartingLoc)
	{
		if (pPlot->isGoody())
		{
			return 0;
		}
		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			if (plotCity(iX, iY, iI) == NULL)
			{
				return 0;
			}
		}
	}
	else
	{
		int iOwnedTiles = 0;
		for (int iI = 0; iI < NUM_CITY_PLOTS_2; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot == NULL || pLoopPlot->isOwned() && pLoopPlot->getTeam() != getTeam())
			{
				iOwnedTiles++;
			}
		}
		if (iOwnedTiles > 10)
		{
			return 0;
		}
	}

	int iBadTile = 0;

	for (int iI = SKIP_CITY_HOME_PLOT; iI < NUM_CITY_PLOTS_2; iI++)
	{
		CvPlot* pLoopPlot = plotCity(iX, iY, iI);

		if (pLoopPlot == NULL)
		{
			iBadTile += 4;
		}
		else if (pLoopPlot->isImpassable(getTeam()))
		{
			iBadTile += 3;
		}
		else if (!pLoopPlot->isFreshWater() && !pLoopPlot->isHills())
		{
			if (pLoopPlot->isWater() && (!bIsCoastal || pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) < 2))
			{
				iBadTile++;
			}
			else if (pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) < 2 || pLoopPlot->calculateTotalBestNatureYield(getTeam()) < 3)
			{
				iBadTile += 2;
			}
		}
		else if (pLoopPlot->isOwned() && pLoopPlot->getTeam() == getTeam() && (pLoopPlot->isCityRadius() || abCitySiteRadius[iI]))
		{
			iBadTile += bAdvancedStart ? 2 : 1;
		}
	}
	iBadTile /= 2;

	if (!bStartingLoc && (iBadTile > 15 || pArea->getNumTiles() <= 2))
	{
		bool bHasGoodBonus = false;

		for (int iI = 0; iI < NUM_CITY_PLOTS_2; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot != NULL && !pLoopPlot->isOwned()
			&& (pLoopPlot->isWater() || (pLoopPlot->area() == pArea) || (pLoopPlot->area()->getCitiesPerPlayer(getID()) > 0)))
			{
				BonusTypes eBonus = pLoopPlot->getBonusType(getTeam());

				if (eBonus != NO_BONUS
				&& (getNumTradeableBonuses(eBonus) == 0 || AI_bonusVal(eBonus) > 10 || GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0))
				{
					bHasGoodBonus = true;
					break;
				}
			}
		}
		if (!bHasGoodBonus)
		{
			return 0;
		}
	}

	// K-Mod - EasyCulture means that it will be easy for us to pop the culture to the 2nd border
	bool bEasyCulture = false;
	int iGreed = 100;

	if (bAdvancedStart)
	{
		iGreed = 150;
	}
	else if (!bStartingLoc)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (hasTrait((TraitTypes)iI))
			{
				//Greedy founding means getting the best possible sites - fitting maximum
				//resources into the fat cross.
				iGreed += (GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier() / 2);
				iGreed += 20 * (GC.getTraitInfo((TraitTypes)iI).getCommerceChange(COMMERCE_CULTURE));
				// K-Mod note: I don't think this is the right way to calculate greed.
				// For example, if greed is high, the civ will end up having fewer, more spread out cities.
				// That's the opposite of what makes an upkeep reduction most useful.
				if (GC.getTraitInfo((TraitTypes)iI).getCommerceChange(COMMERCE_CULTURE) > 0)
				{
					bEasyCulture = true;
				}
			}
		}
		//Fuyu: be greedier
		if (!isFoundedFirstCity())
		{
			//here I'm assuming that the capital gets a palace that has significant culture output, thus popping the final level 2 really quickly
			iGreed = std::max(iGreed, 145);
		}
		else
		{
			for (int iI = 0; (iGreed < 140 && iI < GC.getNumProcessInfos()); iI++)
			{
				if (canMaintain((ProcessTypes)iI))
				{
					iGreed = std::max(iGreed, 100 + 20 * std::min(2, (GC.getProcessInfo((ProcessTypes)iI).getProductionToCommerceModifier(COMMERCE_CULTURE) / 100)));
				}
			}

			for (int iI = 0; (iGreed < 140 && iI < GC.getNumSpecialistInfos()); iI++)
			{
				if (isSpecialistValid((SpecialistTypes)iI))
				{
					iGreed = std::max(iGreed, 100 + 10 * std::min(4, (GC.getSpecialistInfo((SpecialistTypes)iI).getCommerceChange(COMMERCE_CULTURE) + getSpecialistExtraCommerce(COMMERCE_CULTURE))));
				}
			}

			if (iGreed < 140 && AI_isAreaAlone(pArea))
			{
				//assuming that the player actually has access to something that can make a city produce culture
				iGreed = std::max(iGreed, 125 + (5 * std::min(3, pArea->getCitiesPerPlayer(getID()))));
			}
		}
		iGreed = range(iGreed, 100, 150);
	}

	// K-Mod, easy culture
	// culture building process
	if (!bEasyCulture)
	{
		for (int iJ = 0; iJ < GC.getNumProcessInfos(); iJ++)
		{
			if (GET_TEAM(getTeam()).isHasTech(GC.getProcessInfo((ProcessTypes)iJ).getTechPrereq())
			&& GC.getProcessInfo((ProcessTypes)iJ).getProductionToCommerceModifier(COMMERCE_CULTURE) > 0)
			{
				bEasyCulture = true;
				break;
			}
		}
	}
	// free culture building
	if (!bEasyCulture)
	{
		for (int iJ = 0; iJ < GC.getNumBuildingInfos(); iJ++)
		{
			if (isBuildingFree((BuildingTypes)iJ) && GC.getBuildingInfo((BuildingTypes)iJ).getCommerceChange(COMMERCE_CULTURE) > 0)
			{
				bEasyCulture = true;
				break;
			}
		}
	}
	// easy artists
	if (!bEasyCulture)
	{
		for (int iJ = 0; iJ < GC.getNumSpecialistInfos(); iJ++)
		{
			if (isSpecialistValid((SpecialistTypes)iJ) && specialistCommerce((SpecialistTypes)iJ, COMMERCE_CULTURE) > 0)
			{
				bEasyCulture = true;
				break;
			}
		}
	}
	// ! K-Mod

	//iClaimThreshold is the culture required to pop the 2nd borders.
	const int iClaimThreshold =
	(
		std::max(1, GC.getGame().getCultureThreshold((CultureLevelTypes)std::min(2, GC.getNumCultureLevelInfos() - 1)))
		* (bEasyCulture ? 140 : 100)
	);

	int iTakenTiles = 0;
	int iTeammateTakenTiles = 0;
	int iHealth = 0;
	int iValue = 1000;

	int iResourceValue = 0;
	int iSpecialFoodPlus = 0;
	int iSpecialFoodMinus = 0;

	bool bNeutralTerritory = true;

	for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		CvPlot* pLoopPlot = plotCity(iX, iY, iI);

		if (pLoopPlot == NULL)
		{
			iTakenTiles++;
		}
		else if (pLoopPlot->isPlayerCityRadius(getID()) || abCitySiteRadius[iI])
		{
			iTakenTiles++;

			if (abCitySiteRadius[iI])
			{
				iTeammateTakenTiles++;
			}
		}
		else
		{
			int iTempValue = 0;

			int iCultureMultiplier;
			if (!pLoopPlot->isOwned() || (pLoopPlot->getOwner() == getID()))
			{
				iCultureMultiplier = 100;
			}
			else
			{
				bNeutralTerritory = false;
				const int iOtherCulture = std::max(1, pLoopPlot->getCulture(pLoopPlot->getOwner()));

				iCultureMultiplier = 100 * pLoopPlot->getCulture(getID()) + iClaimThreshold;
				iCultureMultiplier /= (100 * iOtherCulture + iClaimThreshold) / 100;

				iCultureMultiplier = std::min(100, iCultureMultiplier);
				//The multiplier is basically normalized...
				//100% means we own (or rightfully own) the tile.
				//50% means the hostile culture is fairly firmly entrenched.
			}

			if (iCultureMultiplier < 50 - 25 * (iNumAreaCities > 0))
			{
				//discourage hopeless cases, especially on other continents.
				iTakenTiles += 2 - (iNumAreaCities > 0);
			}

			const BonusTypes eBonus = pLoopPlot->getBonusType((bStartingLoc) ? NO_TEAM : getTeam());

			const FeatureTypes eFeature = pLoopPlot->getFeatureType();
			int aiYield[NUM_YIELD_TYPES];

			for (int iYieldType = 0; iYieldType < NUM_YIELD_TYPES; ++iYieldType)
			{
				const YieldTypes eYield = (YieldTypes)iYieldType;
				aiYield[eYield] = pLoopPlot->getYield(eYield);

				if (iI == CITY_HOME_PLOT)
				{
					int iBasePlotYield = aiYield[eYield];
					aiYield[eYield] += GC.getYieldInfo(eYield).getCityChange();

					if (eFeature != NO_FEATURE)
					{
						aiYield[eYield] -= GC.getFeatureInfo(eFeature).getYieldChange(eYield);
						iBasePlotYield = std::max(iBasePlotYield, aiYield[eYield]);
					}

					if (eBonus != NO_BONUS)
					{
						const int iBonusYieldChange = GC.getBonusInfo(eBonus).getYieldChange(eYield);
						aiYield[eYield] += iBonusYieldChange;
						iBasePlotYield += iBonusYieldChange;
					}
					aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity());
				}
			}

			if (iI == CITY_HOME_PLOT)
			{
				iTempValue += aiYield[YIELD_FOOD] * (120 + 30 * bStartingLoc);
				iTempValue += aiYield[YIELD_PRODUCTION] * 100;
				iTempValue += aiYield[YIELD_COMMERCE] * 80;
			}
			else
			{
				iTempValue += aiYield[YIELD_FOOD] * (60 + 15 * bStartingLoc);
				iTempValue += aiYield[YIELD_PRODUCTION] * 50;
				iTempValue += aiYield[YIELD_COMMERCE] * 40;
			}
			if (bStartingLoc) // Yield holds much value for new game starting positions.
			{
				iTempValue *= 2;
			}

			if (pLoopPlot->isWater())
			{
				if (aiYield[YIELD_COMMERCE] > 1)
				{
					// Upside is much higher based on multipliers above, with lighthouse a standard coast
					// plot moves up into the higher multiplier category.
					iTempValue += bIsCoastal ? 40 + 10 * aiYield[YIELD_COMMERCE] : -10 * aiYield[YIELD_COMMERCE];

					if (bIsCoastal && aiYield[YIELD_FOOD] >= GC.getFOOD_CONSUMPTION_PER_POPULATION())
					{
						iSpecialFoodPlus += 1;
					}
				}
				if (!bIsCoastal)
				{
					iTempValue -= 400;
				}
			}

			// Favor extra river tiles for eventual building yields, if we are on a river
			if (pLoopPlot->isRiver() && pPlot->isRiver())
			{
				iTempValue += 25 + 10 * GC.getGame().getRiverBuildings();
			}
			if (pLoopPlot->isAsPeak())
			{// Defense bonus...
				iTempValue += 25;
			}

			if (bEasyCulture)
			{
				// 5/4 * 21 ~= 9 * 1.5 + 12 * 1;
				iTempValue *= 5;
				iTempValue /= 4;
			}
			else if (pLoopPlot->getOwner() == getID() || stepDistance(iX, iY, pLoopPlot->getX(), pLoopPlot->getY()) <= 1)
			{
				iTempValue *= 3;
				iTempValue /= 2;
			}
			iTempValue *= iGreed; // (note: see comments about iGreed higher in the code)
			iTempValue /= 100;

			iTempValue *= iCultureMultiplier;
			iTempValue /= 100;

			iValue += iTempValue;

			if (iCultureMultiplier > 33) //ignore hopelessly entrenched tiles.
			{
				if (eFeature != NO_FEATURE && iI != CITY_HOME_PLOT)
				{
					iHealth += GC.getFeatureInfo(eFeature).getHealthPercent();

					iSpecialFoodPlus += std::max(0, aiYield[YIELD_FOOD] - GC.getFOOD_CONSUMPTION_PER_POPULATION());
				}

				if (eBonus != NO_BONUS
				&& (pLoopPlot->area() == pPlot->area() || pLoopPlot->area()->getCitiesPerPlayer(getID()) > 0 || pLoopPlot->isWater()))
				{
					paiBonusCount[eBonus]++;
					FAssert(paiBonusCount[eBonus] > 0);

					iTempValue = AI_bonusVal(eBonus) * (!bStartingLoc && getNumTradeableBonuses(eBonus) == 0 && paiBonusCount[eBonus] == 1 ? 80 : 20);
					iTempValue *= (bStartingLoc ? 100 : iGreed);
					iTempValue /= 100;

					if (iI != CITY_HOME_PLOT && !bStartingLoc
					&& pLoopPlot->getOwner() != getID() && stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) > 1)
					{
						iTempValue *= 2;
						iTempValue /= 3;

						iTempValue *= std::min(150, iGreed);
						iTempValue /= 100;
					}

					if (pLoopPlot->isWater() && !pLoopPlot->isAdjacentToLand())
					{
						iTempValue /= 2;
					}

					iValue += (iTempValue + 10);

					if (iI != CITY_HOME_PLOT && eFeature != NO_FEATURE && GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
					{
						iResourceValue -= 30;
					}
				}
			}
		}
	}

	if (bStartingLoc)
	{
		iResourceValue /= 2;
	}
	iValue += std::max(0, iResourceValue);

	if (iResourceValue < 250 && iTakenTiles > 12)
	{
		return 0;
	}

	if (iTeammateTakenTiles > 1)
	{
		return 0;
	}

	iValue += iHealth / 5;

	if (bStartingLoc && pArea->getNumStartingPlots() == 0)
	{
		iValue += 1000;
	}
	if (bIsCoastal)
	{
		if (bStartingLoc)
		{
			//let other penalties bring this down.
			iValue += 1000;
		}
		else if (pArea->getCitiesPerPlayer(getID()) != 0)
		{
			iValue += 200;

			// Push players to get more coastal cities so they can build navies
			CvArea* pWaterArea = pPlot->waterArea(true);
			if (pWaterArea != NULL)
			{
				iValue += 200;

				if (GET_TEAM(getTeam()).AI_isWaterAreaRelevant(pWaterArea))
				{
					iValue += 200;

					if (countNumCoastalCities() < getNumCities() / 2 || countNumCoastalCitiesByArea(pPlot->area()) == 0)
					{
						iValue *= 250 + 100 * (getNumCities() - countNumCoastalCities()) / getNumCities();
						iValue /= 200;
					}
					// If this location bridges two water areas that's worth a boost
					CvArea* pSecondWaterArea = pPlot->secondWaterArea();
					if (pSecondWaterArea != NULL)
					{
						if (GET_TEAM(getTeam()).AI_isWaterAreaRelevant(pSecondWaterArea))
						{
							iValue *= 4;
							iValue /= 3;
						}
						else // Just bridges a lake or something
						{
							iValue += 50;
						}
					}
					// If this will be our first city on this water area give another boost
					if (countNumCoastalCitiesByArea(pWaterArea) == 0)
					{
						iValue *= 5;
						iValue /= 4;
					}
				}
			}
		}
		else if (bNeutralTerritory)
		{
			iValue += (iResourceValue > 0 ? 800 : 100);
		}
	}

	if (pPlot->isHills())
	{
		iValue += 200;
	}

	if (pPlot->isRiver())
	{
		iValue += 400;
	}

	if (bIsCoastal)
	{
		iValue += 50 * GC.getGame().getCoastalBuildings();
	}

	if (pPlot->isFreshWater())
	{
		iValue += 200;
	}

	if (bStartingLoc)
	{
		const int iRange = GREATER_FOUND_RANGE;
		int iGreaterBadTile = 0;

		foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
		{
			if ((pLoopPlot->isWater() || pLoopPlot->area() == pArea)
			&& plotDistance(iX, iY, pLoopPlot->getX(), pLoopPlot->getY()) <= iRange)
			{
				const int iTempValue =
					(
						13 * pLoopPlot->getYield(YIELD_FOOD) +
						11 * pLoopPlot->getYield(YIELD_PRODUCTION) +
						 7 * pLoopPlot->getYield(YIELD_COMMERCE)
					);
				if (iTempValue < 28)
				{
					iGreaterBadTile += 2;
					if (pLoopPlot->getFeatureType() != NO_FEATURE
					&& pLoopPlot->calculateBestNatureYield(YIELD_FOOD, getTeam()) > 1)
					{
						iGreaterBadTile--;
					}
				}
				iValue += iTempValue;
			}
		}

		iGreaterBadTile /= 2;
		if (iGreaterBadTile > 12)
		{
			iValue *= 11;
			iValue /= iGreaterBadTile;
		}
		int iWaterCount = 0;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot != NULL && pLoopPlot->isWater())
			{
				iWaterCount++;
				if (pLoopPlot->getYield(YIELD_FOOD) <= 1)
				{
					iWaterCount++;
				}
			}
		}
		iWaterCount /= 2;

		int iLandCount = NUM_CITY_PLOTS - iWaterCount;

		if (iLandCount < NUM_CITY_PLOTS / 2)
		{
			//discourage very water-heavy starts.
			iValue *= 1 + iLandCount;
			iValue /= 1 + NUM_CITY_PLOTS / 2;
		}
	}

	if (bStartingLoc)
	{
		if (pPlot->getMinOriginalStartDist() == -1)
		{
			iValue += GC.getMap().maxStepDistance() * 100;
		}
		else
		{
			iValue *= 1 + 4 * pPlot->getMinOriginalStartDist();
			iValue /= 1 + 2 * GC.getMap().maxStepDistance();
		}

		// Nice hacky way to avoid this messing with normalizer, use elsewhere?
		int iMinDistanceFactor = MAX_INT;
		int iMinRange = startingPlotRange();

		iValue *= 100;
		for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
		{
			if (GET_PLAYER((PlayerTypes)iJ).isAlive() && iJ != getID())
			{
				int iClosenessFactor = GET_PLAYER((PlayerTypes)iJ).startingPlotDistanceFactor(pPlot, getID(), iMinRange);
				iMinDistanceFactor = std::min(iClosenessFactor, iMinDistanceFactor);

				if (iClosenessFactor < 1000)
				{
					iValue *= 5000 + iClosenessFactor;
					iValue /= 6000;
				}
			}
		}

		if (iMinDistanceFactor > 1000)
		{
			//give a maximum boost of 20% for somewhat distant locations, don't go overboard.
			iMinDistanceFactor = std::min(1500, iMinDistanceFactor);
			iValue *= 1500 + iMinDistanceFactor;
			iValue /= 2500;
		}
		else if (iMinDistanceFactor < 1000)
		{
			//this is too close so penalize again.
			iValue *= 1000 + iMinDistanceFactor;
			iValue /= 2000;
		}
		iValue /= 100;
	}

	if (bAdvancedStart && pPlot->getBonusType() != NO_BONUS)
	{
		iValue *= 70;
		iValue /= 100;
	}

	CvCity* pNearestCity = GC.getMap().findCity(iX, iY, isNPC() ? NO_PLAYER : getID());

	if (pNearestCity == NULL)
	{
		pNearestCity = GC.getMap().findCity(iX, iY, ((isNPC()) ? NO_PLAYER : getID()), isNPC() ? NO_TEAM : getTeam(), false);
		if (pNearestCity != NULL)
		{
			int iDistance = plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
			iValue -= std::min(500 * iDistance, (8000 * iDistance) / GC.getMap().maxPlotDistance());
		}
	}
	else if (isNPC())
	{
		iValue -= (std::max(0, (8 - plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY()))) * 200);
	}
	else
	{
		int iDistance = plotDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
		int iNumCities = getNumCities();
		if (iDistance > 5)
		{
			iValue -= (iDistance - 5) * 500;
		}
		else if (iDistance < 4)
		{
			iValue -= (4 - iDistance) * 2000;
		}
		iValue *= (8 + iNumCities * 4);
		iValue /= (2 + (iNumCities * 4) + iDistance);
		if (pNearestCity->isCapital())
		{
			iValue *= 150;
			iValue /= 100;
		}
		else if (getCapitalCity() != NULL)
		{
			//Provide up to a 50% boost to value (80% for adv.start)
			//for city sites which are relatively close to the core
			//compared with the most distance city from the core
			//(having a boost rather than distance penalty avoids some distortion)

			//This is not primarly about maitenance but more about empire
			//shape as such forbidden palace/state property are not big deal.
			int iMaxDistanceFromCapital = 0;

			int iCapitalX = getCapitalCity()->getX();
			int iCapitalY = getCapitalCity()->getY();

			foreach_(const CvCity * pLoopCity, cities())
			{
				iMaxDistanceFromCapital = std::max(iMaxDistanceFromCapital, plotDistance(iCapitalX, iCapitalY, pLoopCity->getX(), pLoopCity->getY()));
			}

			FAssert(iMaxDistanceFromCapital > 0);
			iValue *= 100 + (((bAdvancedStart ? 80 : 50) * std::max(0, (iMaxDistanceFromCapital - iDistance))) / iMaxDistanceFromCapital);
			iValue /= 100;
		}
	}

	if (iValue <= 0)
	{
		return 1;
	}

	if (pArea->getNumCities() != 0)
	{
		const int iTeamAreaCities = GET_TEAM(getTeam()).countNumCitiesByArea(pArea);

		if (pArea->getNumCities() == iTeamAreaCities)
		{
			iValue *= 3;
			iValue /= 2;
		}
		else if (pArea->getNumCities() == iTeamAreaCities
			+ GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(pArea)
			+ GET_TEAM(NEANDERTHAL_TEAM).countNumCitiesByArea(pArea))
		{
			iValue *= 4;
			iValue /= 3;
		}
		else if (iTeamAreaCities > 0)
		{
			iValue *= 5;
			iValue /= 4;
		}
	}
	else iValue *= 2;

	if (!bStartingLoc)
	{
		int iFoodSurplus = std::max(0, iSpecialFoodPlus - iSpecialFoodMinus);
		int iFoodDeficit = std::max(0, iSpecialFoodMinus - iSpecialFoodPlus);

		iValue *= 100 + 20 * std::max(0, std::min(iFoodSurplus, 2 * GC.getFOOD_CONSUMPTION_PER_POPULATION()));
		iValue /= 100 + 20 * std::max(0, iFoodDeficit);

		if (getNumCities() > 0)
		{
			int iBonusCount = 0;
			int iUniqueBonusCount = 0;
			for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
			{
				iBonusCount += paiBonusCount[iI];
				iUniqueBonusCount += (paiBonusCount[iI] > 0) ? 1 : 0;
			}
			if (iBonusCount > 4)
			{
				iValue *= 5;
				iValue /= (1 + iBonusCount);
			}
			else if (iUniqueBonusCount > 2)
			{
				iValue *= 5;
				iValue /= (3 + iUniqueBonusCount);
			}
		}
		const int iDeadLockCount = AI_countDeadlockedBonuses(pPlot);
		if (iDeadLockCount > 0)
		{
			if (bAdvancedStart)
				 iValue /= 3 + iDeadLockCount;
			else iValue /= 1 + iDeadLockCount;
		}
	}
	iValue /= 3 + std::max(0, iBadTile - NUM_CITY_PLOTS / 4);

	if (bStartingLoc)
	{
		int iDifferentAreaTile = 0;

		for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(iX, iY, iI);

			if (pLoopPlot == NULL || !pLoopPlot->isWater() && pLoopPlot->area() != pArea)
			{
				iDifferentAreaTile++;
			}
		}
		iValue /= 2 + std::max(0, iDifferentAreaTile - NUM_CITY_PLOTS * 2 / 3);
	}
	return std::max(1, iValue);
}


bool CvPlayerAI::AI_isAreaAlone(const CvArea* pArea) const
{
	return (GET_TEAM(getTeam()).countNumCitiesByArea(pArea) == pArea->getNumCities()
		- GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(pArea)
		- GET_TEAM(NEANDERTHAL_TEAM).countNumCitiesByArea(pArea));
}


bool CvPlayerAI::AI_isCapitalAreaAlone() const
{
	const CvCity* pCapitalCity = getCapitalCity();
	return pCapitalCity ? AI_isAreaAlone(pCapitalCity->area()) : false;
}


bool CvPlayerAI::AI_isPrimaryArea(const CvArea* pArea) const
{
	if (pArea->isWater())
	{
		return false;
	}

	// Toffer
	const int iNumAreaCities = pArea->getCitiesPerPlayer(getID());
	if (iNumAreaCities < 1)
	{
		return false;
	}
	if (iNumAreaCities > 1 + GC.getMap().getWorldSize())
	{
		return true;
	}
	const int iNumCities = getNumCities();

	// Even 1 of 6 should be enough for it to be primary.
	if (iNumCities < 7)
	{
		return true;
	}
	// If 17% or more of my cities are on the landmass, then the landmass is a primary area.
	if (16 < 100 * iNumAreaCities / iNumCities)
	{
		return true;
	}
	// ! Toffer

	const CvCity* pCapitalCity = getCapitalCity();
	return pCapitalCity ? pCapitalCity->area() == pArea : false;
}


int CvPlayerAI::AI_militaryWeight(const CvArea* pArea) const
{
	return 1 + pArea->getPopulationPerPlayer(getID()) + 3 * pArea->getCitiesPerPlayer(getID());
}


int CvPlayerAI::AI_targetCityValue(const CvCity* pCity, bool bRandomize, bool bIgnoreAttackers) const
{
	PROFILE_FUNC();

	CvCity* pNearestCity;
	CvPlot* pLoopPlot;
	int iI;

	FAssertMsg(pCity != NULL, "City is not assigned a valid value");

	int iValue = 1;

	iValue += ((pCity->getPopulation() * (50 + pCity->calculateCulturePercent(getID()))) / 100);

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  06/30/10					 Mongoose & jdog5000	  */
	/*																							  */
	/* War strategy AI																			  */
	/************************************************************************************************/
		// Prefer lower defense
	iValue += std::max(0, (100 - pCity->getDefenseModifier(false)) / 30);

	if (pCity->getDefenseDamage() > 0)
	{
		iValue += ((pCity->getDefenseDamage() / 30) + 1);
	}

	// Significant amounting of borrowing/adapting from Mongoose AITargetCityValueFix
	if (pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
	{
		iValue += 2;
	}

	iValue += 4 * pCity->getNumActiveWorldWonders();

	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHolyCity((ReligionTypes)iI))
		{
			iValue += std::max(2, ((GC.getGame().calculateReligionPercent((ReligionTypes)iI)) / 5));
			iValue += ((AI_getFlavorValue(/* AI_FLAVOR_RELIGION */ (FlavorTypes)1) + 2) / 6);

			if (GET_PLAYER(pCity->getOwner()).getStateReligion() == iI)
			{
				iValue += 2;
			}
			if (getStateReligion() == iI)
			{
				iValue += 8;
			}
			if (GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion() == iI)
			{
				iValue += 1;
			}
		}
	}

	if (pCity->isEverOwned(getID()))
	{
		iValue += 3;

		if (pCity->getOriginalOwner() == getID())
		{
			iValue += 3;
		}
	}
	if (!bIgnoreAttackers)
	{
		iValue += std::min(8, (AI_adjacentPotentialAttackers(pCity->plot()) + 2) / 3);
	}

	for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);

		if (pLoopPlot != NULL)
		{
			if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
			{
				iValue += std::min(8, std::max(1, AI_bonusVal(pLoopPlot->getBonusType(getTeam())) / 10));
			}

			if (pLoopPlot->getOwner() == getID())
			{
				iValue++;
			}

			if (pLoopPlot->isAdjacentPlayer(getID(), true))
			{
				iValue++;
			}
		}
	}

	if (GC.getGame().culturalVictoryValid()
	&& GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3)
	&& pCity->getCultureLevel() >= GC.getGame().culturalVictoryCultureLevel() - 1)
	{
		iValue += 15;

		if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		{
			iValue += 25;

			if (pCity->getCultureLevel() >= (GC.getGame().culturalVictoryCultureLevel()))
			{
				iValue += 10;
			}
		}
	}

	if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
	{
		if (pCity->isCapital())
		{
			iValue += 10;

			if (GET_PLAYER(pCity->getOwner()).AI_isDoVictoryStrategy(AI_VICTORY_SPACE4))
			{
				iValue += 20;

				if (GET_TEAM(pCity->getTeam()).getVictoryCountdown(GC.getGame().getSpaceVictory()) >= 0)
				{
					iValue += 30;
				}
			}
		}
	}

	pNearestCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), getID());

	if (pNearestCity != NULL)
	{
		// Now scales sensibly with map size, on large maps this term was incredibly dominant in magnitude
		int iTempValue = 30;
		iTempValue *= std::max(1, ((GC.getMap().maxStepDistance() * 2) - GC.getMap().calculatePathDistance(pNearestCity->plot(), pCity->plot())));
		iTempValue /= std::max(1, (GC.getMap().maxStepDistance() * 2));

		iValue += iTempValue;
	}

	if (bRandomize)
	{
		iValue += GC.getGame().getSorenRandNum(((pCity->getPopulation() / 2) + 1), "AI Target City Value");
	}
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/

		//	Prefer less defended
	int iDefense = 5 + (static_cast<const CvCityAI*>(pCity))->getGarrisonStrength();
	iDefense = std::max(1, iDefense);
	iValue = ((iValue * 100) / iDefense);

	return iValue;
}


CvCity* CvPlayerAI::AI_findTargetCity(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = 0;
	CvCity* pBestCity = NULL;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (isPotentialEnemy(getTeam(), GET_PLAYER((PlayerTypes)iI).getTeam()))
			{
				foreach_(CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
				{
					if (pLoopCity->area() == pArea)
					{
						const int iValue = AI_targetCityValue(pLoopCity, true);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestCity = pLoopCity;
						}
					}
				}
			}
		}
	}

	return pBestCity;
}


bool CvPlayerAI::AI_isCommercePlot(const CvPlot* pPlot) const
{
	return pPlot->getYield(YIELD_FOOD) >= GC.getFOOD_CONSUMPTION_PER_POPULATION();
}

bool CvPlayerAI::AI_getVisiblePlotDanger(const CvPlot* pPlot, int iRange, bool bAnimalOnly, CvSelectionGroup* group, int acceptableOdds) const
{
	PROFILE_EXTRA_FUNC();
	const CvArea* pPlotArea = pPlot->area();

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange) | filtered(CvPlot::fn::area() == pPlotArea))
	{
		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			// No need to loop over tiles full of our own units
			if (pLoopUnit->getTeam() == getTeam())
			{
				if (!(pLoopUnit->alwaysInvisible()) && (pLoopUnit->getInvisibleType() == NO_INVISIBLE))
				{
					break;
				}
			}

			if (pLoopUnit->isEnemy(getTeam())
			&&
			(
				!bAnimalOnly
				|| pLoopUnit->isAnimal()
				&& pLoopUnit->canAnimalIgnoresBorders()
				&& !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT)
				)
			&& pLoopUnit->canAttack()
			&& !pLoopUnit->isInvisible(getTeam(), false)
			&& pLoopUnit->canEnterOrAttackPlot(pPlot)
			&& (group == NULL || pLoopUnit->getGroup()->AI_attackOdds(pPlot, true, true) > 100 - acceptableOdds))
			{
				return true;
			}
		}
	}

	return false;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  08/20/09								jdog5000	  */
/*																							  */
/* General AI, Efficiency																	   */
/************************************************************************************************/
// Plot danger cache

// The vast majority of checks for plot danger are boolean checks during path planning for non-combat
// units like workers, settlers, and GP.  Since these are simple checks for any danger they can be
// cutoff early if danger is found.  To this end, the two caches tracked are for whether a given plot
// is either known to be safe for the player who is currently moving, or for whether the plot is
// known to be a plot bordering an enemy of this team and therefore unsafe.
//
// The safe plot cache is only for the active moving player and is only set if this is not a
// multiplayer game with simultaneous turns.  The safety cache for all plots is reset when the active
// player changes or a new game is loaded.
//
// The border cache is done by team and works for all game types.  The border cache is reset for all
// plots when war or peace are declared, and reset over a limited range whenever a ownership over a plot
// changes.
bool CvPlayerAI::AI_getAnyPlotDanger(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	if (iRange == -1) iRange = DANGER_RANGE;

	if (bTestMoves && isTurnActive())
	{
		PROFILE("CvPlayerAI::AI_getAnyPlotDanger.ActiveTurn");

		if (iRange <= pPlot->getActivePlayerSafeRangeCache())
		{
			PROFILE("CvPlayerAI::AI_getAnyPlotDanger.NoDangerHit");
			return false;
		}
		if (iRange >= DANGER_RANGE && pPlot->getActivePlayerHasDangerCache())
		{
			PROFILE("CvPlayerAI::AI_getAnyPlotDanger.HasDangerHit");
			return true;
		}
	}
	bool bResult = false;

	const TeamTypes eTeam = getTeam();
	// Exclude cities and defended plots from the hostile border proximity check.
	const bool bCheckBorder = !pPlot->isCity() && pPlot->plotCheck(PUF_canDefend, -1, -1, NULL, getID());

	if (bCheckBorder && iRange >= 2 && pPlot->getBorderDangerCache(eTeam))
	{
		bResult = true;
	}

	// If we have plot danger count here over the same threhold that workers
	//	use to require escorts then consider that dangerous for the AI
	if (!bResult && !isHumanPlayer() && pPlot->getDangerCount(m_eID) > 20)
	{
		bResult = true;
	}

	if (!bResult)
	{
		const CvArea* pPlotArea = pPlot->area();

		foreach_(const CvPlot* plotX, pPlot->rect(iRange, iRange))
		{
			if (plotX->area() == pPlotArea)
			{
				const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), plotX->getX(), plotX->getY());

				if (bCheckBorder && iDistance <= 2 && atWar(plotX->getTeam(), eTeam))
				{
					if (pPlot != plotX)
					{
						pPlot->setBorderDangerCache(eTeam, true);
						plotX->setBorderDangerCache(eTeam, true);
						// Only set the cache for the plotX team if pPlot is owned by us! (ie. owned by their enemy)
						if (pPlot->getTeam() == eTeam) 
						{
							pPlot->setBorderDangerCache(plotX->getTeam(), true);
							plotX->setBorderDangerCache(plotX->getTeam(), true);
						}
					}
					else pPlot->setBorderDangerCache(eTeam, true);

					bResult = true;
					break;
				}

				foreach_(const CvUnit * unitx, plotX->units())
				{
					if (unitx->getTeam() == eTeam && !unitx->alwaysInvisible() && unitx->getInvisibleType() == NO_INVISIBLE)
					{
						break; // No need to loop over tiles where we have regular units
					}

					if (unitx->isEnemy(eTeam)
					&&  unitx->canAttack()
					&& !unitx->isInvisible(eTeam, false)
					&&  unitx->canEnterOrAttackPlot(pPlot))
					{
						if (!bTestMoves)
						{
							bResult = true;
							break;
						}
						else if (iDistance <= unitx->baseMoves() + plotX->isValidRoute(unitx))
						{
							bResult = true;
							break;
						}
					}
				}
			}
		}
	}

	if (GC.getGame().getNumGameTurnActive() == 1 && isTurnActive() && !GC.getGame().isMPOption(MPOPTION_SIMULTANEOUS_TURNS))
	{
		if (bResult)
		{
			if (iRange <= DANGER_RANGE)
			{
				pPlot->setActivePlayerHasDangerCache(true);
			}
		}
		else if (iRange < pPlot->getActivePlayerSafeRangeCache())
		{
			pPlot->setActivePlayerSafeRangeCache(iRange);
		}
	}
	return bResult;
}

#ifdef PLOT_DANGER_CACHING
plotDangerCache CvPlayerAI::plotDangerCache;
int CvPlayerAI::plotDangerCacheHits = 0;
int CvPlayerAI::plotDangerCacheReads = 0;
#endif

int CvPlayerAI::AI_getPlotDanger(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	if (bTestMoves && isTurnActive() && iRange <= pPlot->getActivePlayerSafeRangeCache())
	{
		return 0;
	}

#ifdef PLOT_DANGER_CACHING
#ifdef _DEBUG
	//	Uncomment this to perform functional verification
	//#define VERIFY_PLOT_DANGER_CACHE_RESULTS
#endif

	//	Check cache first
	int worstLRU = 0x7FFFFFFF;

	struct plotDangerCacheEntry* worstLRUEntry = NULL;
	plotDangerCacheReads++;

	//OutputDebugString(CvString::format("AI_yieldValue (%d,%d,%d) at seq %d\n", piYields[0], piYields[1], piYields[2], yieldValueCacheReads).c_str());
	//PROFILE_STACK_DUMP

	for (int i = 0; i < PLOT_DANGER_CACHE_SIZE; i++)
	{
		struct plotDangerCacheEntry* entry = &plotDangerCache.entries[i];
		if (entry->iLastUseCount == 0)
		{
			worstLRUEntry = entry;
			break;
		}

		if (pPlot->getX() == entry->plotX &&
			 pPlot->getY() == entry->plotY &&
			 iRange == entry->iRange &&
			 bTestMoves == entry->bTestMoves)
		{
			entry->iLastUseCount = ++plotDangerCache.currentUseCounter;
			plotDangerCacheHits++;
#ifdef VERIFY_PLOT_DANGER_CACHE_RESULTS
			int realValue = AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);

			if (realValue != entry->iResult)
			{
				FErrorMsg("Plot danger cache verification failure");
			}
#endif
			return entry->iResult;
		}
		else if (entry->iLastUseCount < worstLRU)
		{
			worstLRU = entry->iLastUseCount;
			worstLRUEntry = entry;
		}
	}

	worstLRUEntry->plotX = pPlot->getX();
	worstLRUEntry->plotY = pPlot->getY();
	worstLRUEntry->iRange = iRange;
	worstLRUEntry->bTestMoves = bTestMoves;
	worstLRUEntry->iLastUseCount = ++plotDangerCache.currentUseCounter;
	worstLRUEntry->iResult = AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);

	return worstLRUEntry->iResult;
#else
	return AI_getPlotDangerInternal(pPlot, iRange, bTestMoves);
#endif
}

int CvPlayerAI::AI_getPlotDangerInternal(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_EXTRA_FUNC();
	const CvArea* pPlotArea = pPlot->area();
	const TeamTypes eTeam = getTeam();

	int iCount = 0;
	int iBorderDanger = 0;

	OutputDebugString(CvString::format("AI_getPlotDanger for (%d,%d) at range %d (bTestMoves=%d)\n",
		pPlot->getX(), pPlot->getY(),
		iRange,
		bTestMoves).c_str());

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pLoopPlot->area() == pPlotArea)
		{
			const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());

			if (iDistance <= 2 && atWar(pLoopPlot->getTeam(), eTeam))
			{
				if (iDistance == 0)
				{
					iBorderDanger += 4;
				}
				else if (iDistance == 1)
				{
					iBorderDanger += 2;
				}
				else iBorderDanger++;
			}

			foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
			{
				// No need to loop over tiles full of our own units
				if (pLoopUnit->getTeam() == eTeam
				&& !pLoopUnit->alwaysInvisible()
				&& pLoopUnit->getInvisibleType() == NO_INVISIBLE)
				{
					break;
				}
				if (pLoopUnit->isEnemy(eTeam)
				&& (pLoopUnit->canAttack() || pLoopUnit->plot() == pPlot)
				&& !pLoopUnit->isInvisible(eTeam, false)
				&& (pLoopUnit->canEnterOrAttackPlot(pPlot) || pLoopUnit->plot() == pPlot))
				{
					if (bTestMoves)
					{
						const int iDangerRange = pLoopUnit->baseMoves() + pLoopPlot->isValidRoute(pLoopUnit);

						if (iDangerRange >= iDistance)
						{
							iCount++;
						}
					}
					else iCount++;
				}
			}
		}
	}

	// Note that here we still count border danger in cities - because I want it for AI_cityThreat
	if (iBorderDanger > 0 && (!isHumanPlayer() || pPlot->plotCheck(PUF_canDefend, -1, -1, NULL, getID())))
	{
		iCount += (1 + iBorderDanger) / 2;
	}

	if (GC.getGame().getNumGameTurnActive() == 1 && isTurnActive() && !GC.getGame().isMPOption(MPOPTION_SIMULTANEOUS_TURNS))
	{
		if (iCount > 0)
		{
			if (iRange <= DANGER_RANGE)
			{
				pPlot->setActivePlayerHasDangerCache(true);
			}
		}
		else if (iRange < pPlot->getActivePlayerSafeRangeCache())
		{
			pPlot->setActivePlayerSafeRangeCache(iRange);
		}
	}
	return iCount;
}

// Never used ...
/*
int CvPlayerAI::AI_getUnitDanger(CvUnit* pUnit, int iRange, bool bTestMoves, bool bAnyDanger) const
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	int iCount;
	int iDistance;
	int iBorderDanger;
	int iDX, iDY;

	CvPlot* pPlot = pUnit->plot();
	iCount = 0;
	iBorderDanger = 0;

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	for (iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (iDY = -(iRange); iDY <= iRange; iDY++)
		{
			pLoopPlot = plotXY(pPlot->getX(), pPlot->getY(), iDX, iDY);

			if (pLoopPlot != NULL)
			{
				if (pLoopPlot->area() == pPlot->area())
				{
					iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());
					if (atWar(pLoopPlot->getTeam(), getTeam()))
					{
						if (iDistance == 1)
						{
							iBorderDanger++;
						}
						else if ((iDistance == 2) && (pLoopPlot->isRoute()))
						{
							iBorderDanger++;
						}
					}

					foreach_(CvUnit* pLoopUnit, pLoopPlot->units())
					{
						if (atWar(pLoopUnit->getTeam(), getTeam()))
						{
							if (pLoopUnit->canAttack())
							{
								if (!(pLoopUnit->isInvisible(getTeam(), false)))
								{
									if (pLoopUnit->canEnterOrAttackPlot(pPlot))
									{
										if (!bTestMoves)
										{
											iCount++;
										}
										else
										{
											int iDangerRange = pLoopUnit->baseMoves();
											iDangerRange += ((pLoopPlot->isValidRoute(pLoopUnit)) ? 1 : 0);
											if (iDangerRange >= iDistance)
											{
												iCount++;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (iBorderDanger > 0)
	{
		if (!isHumanPlayer() || pUnit->isAutomated())
		{
			iCount += iBorderDanger;
		}
	}

	return iCount;
}
*/
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

int CvPlayerAI::AI_countNumLocalNavy(const CvPlot* pPlot, int iRange) const
{
	PROFILE_FUNC();

	int iCount = 0;

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pLoopPlot->isWater() || pLoopPlot->getPlotCity() != NULL)
		{
			if (pPlot->area() == pLoopPlot->area() || pPlot->isAdjacentToArea(pLoopPlot->getArea()))
			{
				foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
				{
					const UnitAITypes aiType = pLoopUnit->AI_getUnitAIType();

					if (aiType == UNITAI_ATTACK_SEA || aiType == UNITAI_PIRATE_SEA || aiType == UNITAI_RESERVE_SEA)
					{
						if (pLoopUnit->getTeam() == getTeam())
						{
							iCount++;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_getWaterDanger(const CvPlot* pPlot, int iRange) const
{
	PROFILE_FUNC();

	int iCount = 0;

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pLoopPlot->isWater() && pPlot->isAdjacentToArea(pLoopPlot->getArea()))
		{
			iCount += algo::count_if(pLoopPlot->units(),
				CvUnit::fn::isEnemy(getTeam()) &&
				CvUnit::fn::canAttack() &&
				!CvUnit::fn::isInvisible(getTeam(), false)
			);
		}
	}

	return iCount;
}

bool CvPlayerAI::AI_avoidScience() const
{
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  03/08/10								jdog5000	  */
	/*																							  */
	/* Victory Strategy AI																		  */
	/************************************************************************************************/
	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/
	{
		return true;
	}
	if (isCurrentResearchRepeat())
	{
		return true;
	}

	if (isNoResearchAvailable())
	{
		return true;
	}

	return false;
}

short CvPlayerAI::AI_safeFunding() const
{
	short iSafePercent = GC.getDefineINT("SAFE_PROFIT_MARGIN_BASE_PERCENT");

	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		// Afforess - these calculations mimic the Revolution.py assessment for the revolutions mod (check Revolution.py ~ line 1870)
		// Higher safe percents mean AI has to earn more to be considered "safe"

		const int iRank = GC.getGame().getPlayerRank(getID());
		if (iRank < 3)
		{
			iSafePercent += 5 * (4 - iRank);
		}

		const int iAtWarCount = GET_TEAM(getTeam()).getAtWarCount(true);
		if (iAtWarCount > 0)
		{
			iSafePercent -= 10 + 2 * std::min(iAtWarCount, 5);
		}
		if (isCurrentResearchRepeat())
		{
			iSafePercent *= 2;
			iSafePercent /= 3;
		}
		if (getCommercePercent(COMMERCE_CULTURE) > 70)
		{
			iSafePercent *= 2;
			iSafePercent /= 3;
		}
	}
	else
	{
		const int iWarSuccessRatio = GET_TEAM(getTeam()).AI_getWarSuccessCapitulationRatio();
		if (iWarSuccessRatio < -30)
		{
			iSafePercent -= std::max(20, iWarSuccessRatio / 3);
		}

		if (AI_avoidScience())
		{
			iSafePercent -= 8;
		}

		if (isCurrentResearchRepeat())
		{
			iSafePercent -= 10;
		}
	}

	return iSafePercent;
}

// Toffer - Output mainly range from 0 to 100
//	Values above can signify particulary good funding levels,
//	either from a very large treasury or from positive income at 0-10% taxation.
short CvPlayerAI::AI_fundingHealth(int iExtraExpense, int iExtraExpenseMod) const
{
	PROFILE_FUNC();
	if (isAnarchy() || isNPC())
	{
		return 100;
	}
	int64_t iNetIncome = getCommerceRate(COMMERCE_GOLD) + std::max(0, getGoldPerTurn());
	int64_t iNetExpenses;
	short iProfitMargin = getProfitMargin(iNetExpenses, iExtraExpense, iExtraExpenseMod);
	FASSERT_NOT_NEGATIVE(iProfitMargin);

	//FErrorMsg(CvString::format("iNetIncome=%I64d - iNetExpenses=%I64d - iProfitMargin: %d", iNetIncome, iNetExpenses, (int)iProfitMargin).c_str());

	// Koshling - Never in financial difficulties if we can fund our ongoing expenses with zero taxation
	if (getMinTaxIncome() >= iNetExpenses)
	{
		return 10000; // A magic number in case we want this state to have some kind of significance.
	}
	// Toffer - Things should absolutly be worse off than this before one can claim financial trouble.
	if (iProfitMargin > 25)
	{
		return 200;
	}
	// Toffer - At low to mid tax levels, and with some profit margin to go on, evaluate treasury rather than profit margin.
	if (iProfitMargin > 15 && getCommercePercent(COMMERCE_GOLD) < 50)
	{
		// Toffer - Gamespeed (GS) influence the value of gold, so scale gold treshold to GS, era is exponential factor.
		//	Prehistoric: 25 gold (ultrafast); 100 gold (normal); 1000 gold (eternity)
		//	Ancient: 50 gold (ultrafast); 200 gold (normal); 2000 gold (eternity)
		//	Classical: 125 gold (ultrafast); 500 gold (normal); 5000 gold (eternity)
		const int64_t iEraGoldThreshold = AI_goldTarget();
		int64_t iValue;
		if (iEraGoldThreshold < 1)
		{
			// Return a value based on how many turns we have left before strike happens.
			iValue = 400 * getGold() / (std::max(1, std::abs(calculateGoldRate())) * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent());
		}
		else if (iNetIncome - iNetExpenses >= 0)
		{
			iValue = 100 * getGold() / iEraGoldThreshold;
		}
		else
		{
			// Losing gold per turn, can we keep this up for X number of turns without going below era treshold.
			// X is: 2 (ultrafast); 10 (normal); 100 (eternity). Need more time to react on  slower GS.
			// Koshling - we're never in financial trouble if we can run at current deficits for more than
			//	Toffer - X (GS scaled) turns and stay in healthy territory, so claim full or even excess funding in such a case!
			const int64_t iFutureGoldPrognosis = getGold() + (iNetIncome - iNetExpenses) * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent() / 10;
			iValue = 100 * iFutureGoldPrognosis / iEraGoldThreshold;
		}
		if (iValue > 9999)
		{
			return 10001; // Funding level at 10 000% will be more than adequate to conclude that funding is a non-issue.
		}
		return static_cast<short>(iValue);
	}
	// Toffer - Finances are pretty poor if this point is reached, iProfitMargin is hard to get to a value above 50 in any scenario,
	//	if it is zero it means your expenditure is equal to, or greater than, your income at 100% taxation
	//	if it is *50* it means your expenditure is half the size of your income at 100% taxation.
	//	A value of 100 is close to impossible to reach, as that either means expenditure is zero, or that income is severeal orders of magnitude greater than your expenditure.
	//	Multiplying with 2 may not be needed, but it is to signify that 10% iProfitMargin is not actually all that bad.
	//	Max return value here is 50 as iProfitMargin is guaranteed 25 or less at this point.
	return iProfitMargin * 2;
}


// Calculate a (percentage) modifier the AI can apply to gold to determine how to value it
int CvPlayerAI::AI_goldValueAssessmentModifier() const
{
	// If we're only just funding at the safety level that's not good - rate that as 150% valuation for gold
	return std::max(1, AI_safeFunding() * 100 / std::max<short>(1, AI_fundingHealth()));
}


bool CvPlayerAI::AI_isFinancialTrouble() const
{
	PROFILE_FUNC();
	return !isNPC() && AI_fundingHealth() < AI_safeFunding();
}

int64_t CvPlayerAI::AI_goldTarget() const
{
	PROFILE_EXTRA_FUNC();
	if (getNumCities() < 1 || isNPC())
	{
		return 0;
	}
	const int iEra = GC.getGame().getCurrentEra() + 1;
	const int iModGS = (
		(
			GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent()
			+
			GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent()
			+
			GC.getHandicapInfo(GC.getGame().getHandicapType()).getAITrainPercent()
		)
	);
	int64_t iGold = iEra * (iEra * 2 * getNumCities() + getTotalPopulation()) * iModGS / 300;

	iGold *= getInflationMod10000();
	iGold /= 10000;

	const bool bAnyWar = GET_TEAM(getTeam()).hasWarPlan(true);
	if (bAnyWar)
	{
		iGold *= 3;
		iGold /= 2;
	}

	// Afforess 02/01/10
	if (!GET_TEAM(getTeam()).isGoldTrading() || !GET_TEAM(getTeam()).isTechTrading() || GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING))
	{ // Don't bother saving gold if we can't trade it for anything
		iGold /= 3;
	}
	else if (GC.getGame().isOption(GAMEOPTION_NO_TECH_BROKERING))
	{ // Gold is less useful without tech brokering
		iGold *= 3;
		iGold /= 4;
	}
	// ! Afforess

	if (AI_avoidScience())
	{
		iGold *= 10;
	}
	iGold += AI_goldToUpgradeAllUnits() / (!bAnyWar + 1);

	for (int iI = GC.getNumCorporationInfos() - 1; iI > -1; iI--)
	{
		if (getHasCorporationCount((CorporationTypes)iI) > 0)
		{
			iGold += std::max(0, 2*GC.getCorporationInfo((CorporationTypes)iI).getSpreadCost());
			break;
		}
	}
	return iGold + AI_getExtraGoldTarget();
}

TechTypes CvPlayerAI::AI_bestTech(int iMaxPathLength, bool bIgnoreCost, bool bAsync, TechTypes eIgnoreTech, AdvisorTypes eIgnoreAdvisor)
{
	PROFILE("CvPlayerAI::AI_bestTech");
	logBBAI("Begin best tech evaluation...");

	int iValue;
	int iBestValue = 0;
	TechTypes eBestTech = NO_TECH;
	TechTypes eFirstTech = NO_TECH;
	int iPathLength;
	const CvTeam& kTeam = GET_TEAM(getTeam());

	// Afforess 08/09/10
	// Forces AI to Beeline for Religious Techs if they have no religions
	bool bValid = GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS);
	if (!bValid)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (hasTrait((TraitTypes)iI) && GC.getTraitInfo((TraitTypes)iI).getMaxAnarchy() >= 0)
			{
				bValid = true;
				break;
			}
		}
	}
	if (bValid)
	{
		if (getCommercePercent(COMMERCE_RESEARCH) < 90)
		{
			bValid = false;
		}
		if (countHolyCities() > 0 && (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION) || GC.getGame().isOption(GAMEOPTION_RELIGION_LIMITED)))
		{
			bValid = false;
		}
		if (!GET_TEAM(getTeam()).hasWarPlan(true))
		{
			bValid = false;
		}
		if (getNumCities() == 1)
		{
			bValid = false;
		}
	}
	if (bValid)
	{
		eBestTech = AI_bestReligiousTech(iMaxPathLength * 3, eIgnoreTech, eIgnoreAdvisor);
		if (eBestTech != NO_TECH)
		{
			//	Don't retain the beeline persistently since we need to re-evaluate
			//	each turn in case someone has beaten us to it for religions
			return eBestTech;
		}
	}
	// ! Afforess

	// If we had already decided to beeline previously, stick with it
	if (m_eBestResearchTarget != NO_TECH && iMaxPathLength > 1)
	{
		if (canResearch(m_eBestResearchTarget, false))
		{
			techPath* path = findBestPath(m_eBestResearchTarget, iValue, bIgnoreCost, bAsync);

			eFirstTech = findStartTech(path);

			if (gPlayerLogLevel >= 1)
			{
				if (eFirstTech != NO_TECH)
				{
					logBBAI("  Player %d (%S) continues toward tech %S, start tech %S", getID(), getCivilizationDescription(0), GC.getTechInfo(m_eBestResearchTarget).getDescription(), GC.getTechInfo(eFirstTech).getDescription());
				}
				else
				{
					logBBAI("  Player %d (%S) continues toward tech %S, start tech NONE", getID(), getCivilizationDescription(0), GC.getTechInfo(m_eBestResearchTarget).getDescription());
				}
			}
			return eFirstTech;
		}
	}

#ifdef DEBUG_TECH_CHOICES
	CvWString szPlayerName = getName();
	DEBUGLOG("AI_bestTech:%S\n", szPlayerName.GetCString());
#endif

	bool beeLine = false;
	int beeLineThreshold;

	do
	{
		for (int iI = 0; iI < GC.getNumTechInfos(); iI++)
		{
			if (eIgnoreTech == NO_TECH || iI != eIgnoreTech)
			{
				const TechTypes eTechX = static_cast<TechTypes>(iI);

				if ((eIgnoreAdvisor == NO_ADVISOR || GC.getTechInfo(eTechX).getAdvisorType() != eIgnoreAdvisor)
				&& canResearch(eTechX, false)
				&& GC.getTechInfo(eTechX).getEra() <= getCurrentEra() + 1)
				{
					iPathLength = findPathLength(eTechX, false);

					bool bValid = false;

					if (!beeLine)
					{
						bValid = iPathLength <= iMaxPathLength;
					}
					else if (iPathLength > iMaxPathLength && iPathLength <= iMaxPathLength * 7)
					{
						const int iTempValue = AI_TechValueCached(eTechX, bAsync);

						bValid = iTempValue * 100 / GC.getTechInfo(eTechX).getResearchCost() > iBestValue;

						if (bValid)
							logBBAI("  Beelining worth examining tech %S (val %d)", GC.getTechInfo(eTechX).getDescription(), (iTempValue * 100) / GC.getTechInfo(eTechX).getResearchCost());
						else logBBAI("  Beelining rejects examination of tech %S (val %d)", GC.getTechInfo(eTechX).getDescription(), (iTempValue * 100) / GC.getTechInfo(eTechX).getResearchCost());
					}

					if (bValid)
					{
						techPath* path = findBestPath(eTechX, iValue, bIgnoreCost, bAsync);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eBestTech = eTechX;
							eFirstTech = findStartTech(path);
						}
						delete path;
					}
				}
			}
		}

		// Don't beeline for async human advisor calls to this method and especially don't get a synced rand to decide if you want to beeline
		if (bAsync)
			break;

		//	Most of the time check for good bee-lines.  The probability is so high because the AI will
		//	re-evaluate every time it earns a tech so it will re-decide to beeline (or not) each time
		//	Barbarians are stupid and never beeline
		if (isNPC() || iMaxPathLength == 1 || GC.getGame().getSorenRandNum(8, "AI tech beeline") == 0)
		{
			break;
		}

		if (!beeLine && eBestTech != NO_TECH) //Afforess check against NO_TECH (or else CTD...)
		{
			logBBAI("  Non-beeline tech choice is %S with value/cost %d - checking beeline possibilities...", GC.getTechInfo(eBestTech).getDescription(), iBestValue);
		}
		beeLineThreshold = iBestValue;
		beeLine = !beeLine;
	} while (beeLine);

	if (gPlayerLogLevel >= 1 && eBestTech != NO_TECH && eFirstTech != NO_TECH)
	{
		logBBAI("  Player %d (%S) selects tech %S with value/cost %d, start tech %S", getID(), getCivilizationDescription(0), GC.getTechInfo(eBestTech).getDescription(), iBestValue, GC.getTechInfo(eFirstTech).getDescription());
	}

	if (iMaxPathLength > 1)
	{
		//	Only cache tragets generated from non-immediate best-tech searches
		m_eBestResearchTarget = eBestTech;
	}

	return eFirstTech;
}

struct TechResearchDist
{
	TechResearchDist(TechTypes tech = NO_TECH, int dist = 0) : tech(tech), dist(dist) {}

	bool operator<(const TechResearchDist& other) const
	{
		return dist < other.dist;
	}

	TechTypes tech;
	int dist;
};

//	Calculate an estimate of the value of the average tech amongst those we could currently research
//	for performance reasons we just sample rather than measuring all possibilities
int CvPlayerAI::AI_averageCurrentTechValue(TechTypes eRelativeTo, bool bAsync)
{
	PROFILE_EXTRA_FUNC();
	const size_t MAX_SAMPLE_SIZE = 4;
	const CvTeamAI& team = GET_TEAM(getTeam());

	int iCost = team.getResearchCost(eRelativeTo);

	//	Determine the sample to use - we use the researchable techs closest in base cost to the one we are seeking to compare with the average
	std::vector<TechResearchDist> researchCosts;
	foreach_(const TechTypes eTechX, team.getAdjacentResearch())
	{
		FAssertMsg(canResearch(eTechX, true, false), CvString::format("team %d - tech: %S (%d)", getTeam(), GC.getTechInfo(eTechX).getDescription(), (int)eTechX).c_str());
		if (eTechX != eRelativeTo && canResearch(eTechX))
		{
			researchCosts.push_back(TechResearchDist(eTechX, std::abs(team.getResearchCost(eTechX) - iCost)));
		}
	}

	// We couldn't find any techs to sample so return early
	if (researchCosts.empty())
	{
		return AI_TechValueCached(eRelativeTo, bAsync);
	}
	// Sort for closest first
	algo::sort(researchCosts);
	researchCosts.resize(std::min(researchCosts.size(), MAX_SAMPLE_SIZE));

	int iTotal = 0;
	int iDivisor = 1;
	foreach_(const TechResearchDist & itr, researchCosts)
	{
		const int iValue = AI_TechValueCached(itr.tech, bAsync);

		while (MAX_INT - iTotal < iValue / iDivisor)
		{
			iTotal /= 2;
			iDivisor *= 2;
		}

		iTotal += iValue / iDivisor;
	}

	return (iTotal / researchCosts.size()) * iDivisor;
}

int CvPlayerAI::AI_TechValueCached(TechTypes eTech, bool bAsync, bool considerFollowOns)
{
	PROFILE_FUNC();

	int iValue;

	TechTypesValueMap::const_iterator techValueItr = m_cachedTechValues.find(eTech);
	if (techValueItr == m_cachedTechValues.end())
	{
		PROFILE("CvPlayerAI::AI_TechValueCached.CacheMiss");

		if (gPlayerLogLevel > 2)
		{
			logBBAI("Player %d (%S) calculate value for tech %S (cache miss)",
					getID(),
					getCivilizationDescription(0),
					GC.getTechInfo(eTech).getDescription());
		}

		iValue = AI_techValue(eTech, findPathLength(eTech, false), true, bAsync);

		if (!bAsync)
		{
			m_cachedTechValues[eTech] = iValue;
		}
	}
	else
	{
		iValue = m_cachedTechValues[eTech];
	}

	if (considerFollowOns)
	{
		int iTotalWeight = 100;

		// What does it (immediately) lead to?
		foreach_(const TechTypes eLeadsTo, GC.getTechInfo(eTech).getLeadsToTechs())
		{
			bool bIsORPreReq = false;
			foreach_(const TechTypes ePrereqOr, GC.getTechInfo(eLeadsTo).getPrereqOrTechs())
			{
				if (ePrereqOr == eTech)
				{
					bIsORPreReq = true;
				}
				else if (GET_TEAM(getTeam()).isHasTech(ePrereqOr))
				{
					// Already got an OR pre-req, another makes no difference
					bIsORPreReq = false;
					break;
				}
			}

			bool bIsANDPreReq = false;
			int iANDPrereqs = 0;
			foreach_(const TechTypes ePrereqAnd, GC.getTechInfo(eLeadsTo).getPrereqAndTechs())
			{
				if (!GET_TEAM(getTeam()).isHasTech(ePrereqAnd))
				{
					iANDPrereqs++;
					if (ePrereqAnd == eTech)
					{
						bIsANDPreReq = true;
					}
				}
			}

			if (bIsORPreReq || bIsANDPreReq)
			{
				// Consider all the AND pre-reqs as worth 33% of the follow on, and significant OR as 25%
				const int iANDPercentage = (bIsANDPreReq ? 33 / iANDPrereqs : 0);
				const int iORPercentage = (bIsORPreReq ? 25 : 0);

				iTotalWeight += iANDPercentage + iORPercentage;

				iValue += (iANDPercentage + iORPercentage) * AI_TechValueCached(eLeadsTo, bAsync) / 100;
			}
		}

		//	Normalize to an average to make it comparable with a tech evaluated without follow-ons
		while (iValue > MAX_INT / 100)
		{
			iValue /= 2;
			iTotalWeight /= 2;

			if (iTotalWeight == 0)
			{
				iTotalWeight = 1;
			}
		}
		iValue = iValue * 100 / iTotalWeight;
	}

	return iValue;
}

int CvPlayerAI::techPathValuePerUnitCost(techPath* path, TechTypes eTech, bool bIgnoreCost, bool bAsync)
{
	PROFILE_EXTRA_FUNC();
	int	iCost = 0;
	int	iValue = 0;
	int iScaleFactor = 1;

	if (gPlayerLogLevel > 1)
	{
		logBBAI("  Evaluate tech path value:");
	}
	foreach_(const TechTypes & loopTech, *path)
	{
		int iTempCost = std::max(1, GET_TEAM(getTeam()).getResearchCost(eTech) - GET_TEAM(getTeam()).getResearchProgress(eTech));
		int iTempValue = AI_TechValueCached(loopTech, bAsync);

		if (gPlayerLogLevel > 2)
		{
			logBBAI("	tech %S: cost %d, value %d", GC.getTechInfo(loopTech).getDescription(), iTempCost, iTempValue);
		}
		iCost += iTempCost;
		iValue += iTempValue / iScaleFactor;

		while (iValue > MAX_INT / 100)
		{
			iScaleFactor *= 2;
			iValue /= 2;
		}
	}

	int iCostFactor = iCost;
	int iIterator = ((int)GC.getGame().getCurrentEra() - 1);
	for (int iI = 0; iI < iIterator; iI++)
	{
		iCostFactor /= 2;
	}
	iCostFactor = std::max(1, iCostFactor);
	iValue = std::max(1, (100 * iValue) / (bIgnoreCost ? 1 : iCostFactor));

	if (iValue > MAX_INT / iScaleFactor)
	{
		return MAX_INT;
	}
	else
	{
		return iValue * iScaleFactor;
	}
}

techPath* CvPlayerAI::findBestPath(TechTypes eTech, int& valuePerUnitCost, bool bIgnoreCost, bool bAsync)
{
	PROFILE_FUNC();

	std::vector<techPath*> possiblePaths;
	techPath* initialSeed = new techPath();

	possiblePaths.push_back(initialSeed);

	constructTechPathSet(eTech, possiblePaths, *initialSeed);

	//	Find the lowest cost of the possible paths
	int	iBestValue = 0;
	techPath* bestPath = NULL;

	foreach_(techPath * path, possiblePaths)
	{
		const int iValue = techPathValuePerUnitCost(path, eTech, bIgnoreCost, bAsync);
		if (gPlayerLogLevel > 2)
		{
			logBBAI("  Evaluated tech path value leading to %S as %d", GC.getTechInfo(eTech).getDescription(), iValue);
		}
		if (iValue >= iBestValue)
		{
			iBestValue = iValue;
			bestPath = path;
		}
	}

	foreach_(const techPath * path, possiblePaths)
	{
		if (path != bestPath)
		{
			delete path;
		}
	}

	valuePerUnitCost = iBestValue;
	return bestPath;
}

TechTypes CvPlayerAI::findStartTech(techPath* path) const
{
	PROFILE_EXTRA_FUNC();
	foreach_(const TechTypes & tech, *path)
	{
		if (canResearch(tech))
		{
			return tech;
		}
	}

	return NO_TECH;
}


void CvPlayerAI::resetBonusClassTallyCache(const int iTurn, const bool bFull)
{
	PROFILE_EXTRA_FUNC();
	if (m_iBonusClassTallyCachedTurn != iTurn)
	{
		if (bFull)
		{
			for (int iI = GC.getNumBonusClassInfos() - 1; iI > -1; iI--)
			{
				m_bonusClassRevealed[iI] = 0;
				m_bonusClassUnrevealed[iI] = 0;
				m_bonusClassHave[iI] = 0;
			}
			const CvTeam& team = GET_TEAM(getTeam());

			for (int iI = GC.getNumBonusInfos() - 1; iI > -1; iI--)
			{
				const CvBonusInfo& bonus = GC.getBonusInfo((BonusTypes)iI);
				if (bonus.getTechReveal() != NO_TECH)
				{
					if (team.isHasTech((TechTypes)bonus.getTechReveal()))
					{
						m_bonusClassRevealed[bonus.getBonusClassType()]++;
					}
					else m_bonusClassUnrevealed[bonus.getBonusClassType()]++;


					if (getNumAvailableBonuses((BonusTypes)iI) > 0 || countOwnedBonuses((BonusTypes)iI) > 0)
					{
						m_bonusClassHave[bonus.getBonusClassType()]++;
					}
				}
			}
		}
		m_iBonusClassTallyCachedTurn = iTurn;
	}
}

int CvPlayerAI::AI_techValue(TechTypes eTech, int iPathLength, bool bIgnoreCost, bool bAsync)
{
	PROFILE_FUNC();

	resetBonusClassTallyCache(GC.getGame().getGameTurn());

	const CvTechInfo& kTech = GC.getTechInfo(eTech);
	const CvTeam& kTeam = GET_TEAM(getTeam());

	if (gPlayerLogLevel > 2)
	{
		logBBAI("Player %d (%S) calculate value for tech %S", getID(), getCivilizationDescription(0), kTech.getDescription());
	}
	CvCity* pCapitalCity = getCapitalCity();

	bool bCapitalAlone = (GC.getGame().getElapsedGameTurns() > 0) ? AI_isCapitalAreaAlone() : false;
	bool bFinancialTrouble = AI_isFinancialTrouble();
	bool bAdvancedStart = getAdvancedStartPoints() >= 0;

	int iHasMetCount = kTeam.getHasMetCivCount(true);
	int iCoastalCities = countNumCoastalCities();
	int iConnectedForeignCities = countPotentialForeignTradeCitiesConnected();

	const int iCityCount = getNumCities();
	const int iRCSMultiplier = 1 + GC.getGame().isOption(GAMEOPTION_CULTURE_REALISTIC_SPREAD);

	int iValue = 0;

	int iRandomMax = 2000;

	// Map stuff
	if (iCoastalCities > 0 && kTech.isExtraWaterSeeFrom())
	{
		iValue += 100 * iRCSMultiplier;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	if (kTech.isMapCentering())
	{
		iValue += 100;
	}

	if (kTech.isMapVisible())
	{
		iValue += 100;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	// Expand trading options
	if (kTech.isMapTrading())
	{
		iValue += 100;

		if (bCapitalAlone)
		{
			iValue += 400;
		}
	}

	if (kTech.isTechTrading() && !GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING))
	{
		iValue += 500;

		iValue += 500 * iHasMetCount;
	}

	if (kTech.isGoldTrading())
	{
		iValue += 200;

		if (iHasMetCount > 0)
		{
			iValue += 400;
		}
	}

	if (kTech.isOpenBordersTrading() && iHasMetCount > 0)
	{
		iValue += 500;

		if (iCoastalCities > 0)
		{
			iValue += 400;
		}

		if (isMinorCiv() && GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_START_AS_MINORS))
		{
			iValue += 250 + 120 * iHasMetCount;
		}
	}

	if (kTech.isDefensivePactTrading())
	{
		iValue += 400;
	}

	if (kTech.isPermanentAllianceTrading() && GC.getGame().isOption(GAMEOPTION_ENABLE_PERMANENT_ALLIANCES))
	{
		iValue += 200;
	}

	if (kTech.isVassalStateTrading() && !GC.getGame().isOption(GAMEOPTION_NO_VASSAL_STATES))
	{
		iValue += 200;
	}

	// Tile improvement abilities
	if (kTech.isBridgeBuilding())
	{
		iValue += 400 * iRCSMultiplier;
	}

	if (kTech.isIrrigation())
	{
		iValue += 400;
	}

	if (kTech.isIgnoreIrrigation())
	{
		iValue += 500;
	}

	if (kTech.isWaterWork())
	{
		iValue += (600 * iCoastalCities);
	}

	if (kTech.isCanPassPeaks())
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			const CvPlot* pPlot = GC.getMap().plotByIndex(iI);
			if (pPlot->isAsPeak() && pPlot->getOwner() != NO_PLAYER
			&& GET_PLAYER(pPlot->getOwner()).getID() == getID())
			{
				iValue += 35 * iRCSMultiplier;
			}
		}
	}

	if (kTech.isMoveFastPeaks())
	{
		iValue += 150 * iRCSMultiplier;
	}

	if (kTech.isCanFoundOnPeaks())
	{
		iValue += 100 * iRCSMultiplier;
	}

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tMisc value: %d", iValue);
	}

	int iTempValue = 0;
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (GC.getCorporationInfo((CorporationTypes)iI).getObsoleteTech() != NO_TECH
		&& GC.getCorporationInfo((CorporationTypes)iI).getObsoleteTech() == eTech)
		{
			foreach_(const CvCity * pLoopCity, cities())
			{
				if (pLoopCity->isHasCorporation((CorporationTypes)iI))
				{
					iTempValue -= AI_corporationValue((CorporationTypes)iI, pLoopCity);
				}
			}
		}
	}
	iValue += iTempValue / 1000;

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tCorporation value: %d", iTempValue / 1000);
	}

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if (GC.getPromotionInfo((PromotionTypes)iI).getObsoleteTech() != NO_TECH
		&& GC.getPromotionInfo((PromotionTypes)iI).getObsoleteTech() == eTech)
		{
			foreach_(const CvUnit * pLoopUnit, units())
			{
				if (pLoopUnit->isHasPromotion((PromotionTypes)iI))
				{
					iTempValue -= AI_promotionValue((PromotionTypes)iI, pLoopUnit->getUnitType(), pLoopUnit, pLoopUnit->AI_getUnitAIType());
				}
			}
		}
	}
	iValue += iTempValue / 100;

	if (gPlayerLogLevel > 2) logBBAI("\tPromotion value: %d", iTempValue / 100);

	iTempValue = 0;

	if (kTech.isRebaseAnywhere() && GC.getMAX_AIRLIFT_RANGE() > 0)
	{
		iValue += 300;
	}

	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		if (kTech.getFreeSpecialistCount(iI) != 0)
		{
			iValue += 50 * getNumCities() * kTech.getFreeSpecialistCount(iI);
		}
	}

	iValue += (kTech.getFeatureProductionModifier() * 2);
	iValue += (kTech.getWorkerSpeedModifier() * 4);
	iValue += (kTech.getTradeRoutes() * (std::max((getNumCities() + 2), iConnectedForeignCities) + 1) * ((bFinancialTrouble) ? 200 : 100));

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION4))
	{
		iValue += (kTech.getHealth() * 350);
	}
	else
	{
		iValue += (kTech.getHealth() * 200);
	}

	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++)
	{
		iValue += -(GC.getRouteInfo((RouteTypes)iI).getTechMovementChange(eTech) * 100);
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		iValue += (kTech.getDomainExtraMoves(iI) * 200);
	}

	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		if (kTech.isCommerceFlexible(iI))
		{
			iValue += 100;
			if (iI == COMMERCE_CULTURE && AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
			{
				iValue += 1000;
			}
		}
	}

	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
	{
		if (kTech.isTerrainTrade(iI))
		{
			if (GC.getTerrainInfo((TerrainTypes)iI).isWaterTerrain())
			{
				if (pCapitalCity != NULL)
				{
					iValue += (countPotentialForeignTradeCities(pCapitalCity->area()) * 100);
				}

				if (iCoastalCities > 0)
				{
					iValue += ((bCapitalAlone) ? 950 : 350) * iRCSMultiplier;
				}

				iValue += 50;
			}
			else iValue += 1000;
		}
	}

	if (kTech.isRiverTrade())
	{
		iValue += 1000 * iRCSMultiplier;
	}

	/* ------------------ Tile Improvement Value  ------------------ */
	int iTileImprovementValue = 0;
	for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++)
	{
		for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
		{
			iTempValue = 0;

			/* original code
			iTempValue += (GC.getImprovementInfo((ImprovementTypes)iI).getTechYieldChanges(eTech, iK) * getImprovementCount((ImprovementTypes)iI) * 50); */
			// Often, an improvment only becomes viable after it gets the tech bonus.
			// So it's silly to score the bonus proportionally to how many of the improvements we already have.
			iTempValue += (GC.getImprovementInfo((ImprovementTypes)iI).getTechYieldChanges(eTech, iK) * (getImprovementCount((ImprovementTypes)iI) + 2 * getNumCities()) * 35);
			// This new version is still bork, but at least it won't be worthless.

			iTempValue *= AI_yieldWeight((YieldTypes)iK);
			iTempValue /= 100;

			iTileImprovementValue += iTempValue;
		}
	}

	iValue += iTileImprovementValue;

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tTile improvement value: %d", iTileImprovementValue);
	}

	//ls612: Tech Commerce Modifiers
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		if (kTech.getCommerceModifier(iI) != 0)
		{
			iValue += (kTech.getCommerceModifier(iI) * 100);
		}

		//Extra check for financially challenged AIs
		if (iI == 0 && kTech.getCommerceModifier(iI) < 0 && AI_isFinancialTrouble())
		{
			iValue -= 100;
		}
	}

	int iBuildValue = 0;
	for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
	{
		if (GC.getBuildInfo((BuildTypes)iJ).getTechPrereq() == eTech)
		{
			ImprovementTypes eImprovement = GC.getBuildInfo((BuildTypes)iJ).getImprovement();

			if (eImprovement != NO_IMPROVEMENT)
			{
				//	If it's an upgradable improvement
				eImprovement = kTeam.finalImprovementUpgrade(eImprovement);
			}
			else
			{
				// only increment build value if it is not an improvement, otherwise handle it there
				iBuildValue += 200;
			}

			if (eImprovement != NO_IMPROVEMENT)
			{
				const CvImprovementInfo& kImprovement = GC.getImprovementInfo(eImprovement);

				int iImprovementValue = 300;

				iImprovementValue += ((kImprovement.isActsAsCity()) ? 75 : 0);
				iImprovementValue += ((kImprovement.isHillsMakesValid()) ? 100 : 0);
				iImprovementValue += ((kImprovement.isFreshWaterMakesValid()) ? 200 : 0);
				iImprovementValue += ((kImprovement.isRiverSideMakesValid()) ? 100 : 0);
				iImprovementValue += ((kImprovement.isCarriesIrrigation()) ? 300 : 0);

				for (int iK = 0; iK < GC.getNumTerrainInfos(); iK++)
				{
					iImprovementValue += (kImprovement.getTerrainMakesValid(iK) ? 50 : 0);

					//Desert has negative defense // Toffer - Not very well defined then...
					if (GC.getTerrainInfo((TerrainTypes)iK).getDefenseModifier() < 0 && kTeam.isCanFarmDesert())
					{
						iImprovementValue += 50;
					}
				}

				for (int iK = 0; iK < GC.getNumFeatureInfos(); iK++)
				{
					iImprovementValue += (kImprovement.getFeatureMakesValid(iK) ? 50 : 0);
				}

				for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
				{
					iTempValue = 0;

					iTempValue += (kImprovement.getYieldChange(iK) * 200);
					iTempValue += (kImprovement.getRiverSideYieldChange(iK) * 100);
					iTempValue += (kImprovement.getIrrigatedYieldChange(iK) * 150);

					// land food yield is more valueble
					if (iK == YIELD_FOOD && !kImprovement.isWaterImprovement())
					{
						iTempValue *= 3;
						iTempValue /= 2;
					}

					if (bFinancialTrouble && iK == YIELD_COMMERCE)
					{
						iTempValue *= 2;
					}

					iTempValue *= AI_yieldWeight((YieldTypes)iK);
					iTempValue /= 100;

					iImprovementValue += iTempValue;
				}

				for (int iK = 0; iK < GC.getNumBonusInfos(); iK++)
				{
					int iBonusValue = 0;

					iBonusValue += (kImprovement.isImprovementBonusMakesValid(iK) ? 450 : 0);
					iBonusValue += (kImprovement.isImprovementObsoleteBonusMakesValid(iK) ? 100 : 0);
					iBonusValue += (kImprovement.isImprovementBonusTrade(iK) ? 45 * AI_bonusVal((BonusTypes)iK) : 0);

					if (iBonusValue > 0)
					{
						for (int iL = 0; iL < NUM_YIELD_TYPES; iL++)
						{
							iTempValue = 0;

							iTempValue += (kImprovement.getImprovementBonusYield(iK, iL) * 300);
							iTempValue += (kImprovement.getIrrigatedYieldChange(iL) * 200);

							// food bonuses are more valuable
							if (iL == YIELD_FOOD)
							{
								iTempValue *= 2;
							}
							// otherwise, devalue the bonus slightly
							else if (iL == YIELD_COMMERCE && bFinancialTrouble)
							{
								iTempValue *= 4;
								iTempValue /= 3;
							}
							else
							{
								iTempValue *= 3;
								iTempValue /= 4;
							}

							if (bAdvancedStart && getCurrentEra() < 2)
							{
								iValue *= (iL == YIELD_FOOD) ? 3 : 2;
							}

							iTempValue *= AI_yieldWeight((YieldTypes)iL);
							iTempValue /= 100;

							iBonusValue += iTempValue;
						}

						const int iNumBonuses = countOwnedBonuses((BonusTypes)iK);

						if (iNumBonuses > 0)
						{
							iBonusValue *= (iNumBonuses + 2);

							//Fuyu: massive bonus for early worker logic
							int iCityRadiusBonusCount = 0;
							if (getNumCities() <= 3 && GC.getGame().getElapsedGameTurns() < 30 * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent() / 100)
							{
								//count bonuses inside city radius or easily claimed
								foreach_(const CvCity * pLoopCity, cities())
								{
									iCityRadiusBonusCount += pLoopCity->AI_countNumBonuses((BonusTypes)iK, true, pLoopCity->getCommerceRate(COMMERCE_CULTURE) > 0, -1);
								}
							}
							if (iCityRadiusBonusCount > 1)
							{
								iTempValue *= 3 + iCityRadiusBonusCount - getNumCities();
							}
							iBonusValue /= kImprovement.isWaterImprovement() ? 4 : 3; // water resources are worthless

							iImprovementValue += iBonusValue;
						}
					}
				}

				// if water improvement, weight by coastal cities (weight the whole build)
				if (kImprovement.isWaterImprovement())
				{
					iImprovementValue *= iCoastalCities;
					iImprovementValue /= std::max(1, iCityCount / 2);
				}

				iBuildValue += iImprovementValue;
			}

			const RouteTypes eRoute = (RouteTypes)GC.getBuildInfo((BuildTypes)iJ).getRoute();

			if (eRoute != NO_ROUTE)
			{
				iBuildValue += ((getBestRoute() == NO_ROUTE) ? 700 : 200) * (getNumCities() + (bAdvancedStart ? 4 : 0));

				for (int iK = 0; iK < NUM_YIELD_TYPES; iK++)
				{
					iTempValue = GC.getRouteInfo(eRoute).getYieldChange(iK) * 100;

					for (int iL = 0; iL < GC.getNumImprovementInfos(); iL++)
					{
						iTempValue += GC.getImprovementInfo((ImprovementTypes)iL).getRouteYieldChanges(eRoute, iK) * 50;
					}
					iTempValue *= AI_yieldWeight((YieldTypes)iK);
					iTempValue /= 100;

					iBuildValue += iTempValue;
				}
			}
		}
	}

	//the way feature-remove is done in XML is pretty weird
	//I believe this code needs to be outside the general BuildTypes loop
	//to ensure the feature-remove is only counted once rather than once per build
	//which could be a lot since nearly every build clears jungle...

	//TB Note: I'm thinking buildinfo feature tech is NOT the right call here at all?
	for (int iJ = 0; iJ < GC.getNumFeatureInfos(); iJ++)
	{
		bool bIsFeatureRemove = false;
		for (int iK = 0; iK < GC.getNumBuildInfos(); iK++)
		{
			if (GC.getBuildInfo((BuildTypes)iK).getFeatureTech((FeatureTypes)iJ) == eTech)
			{
				bIsFeatureRemove = true;
				break;
			}
		}

		if (bIsFeatureRemove)
		{
			iBuildValue += 100;

			//Fuyu - Tech Value for Feature Remove - bonus for early worker logic
			if ((GC.getFeatureInfo(FeatureTypes(iJ)).getHealthPercent() < 0) ||
				((GC.getFeatureInfo(FeatureTypes(iJ)).getYieldChange(YIELD_FOOD) + GC.getFeatureInfo(FeatureTypes(iJ)).getYieldChange(YIELD_PRODUCTION) + GC.getFeatureInfo(FeatureTypes(iJ)).getYieldChange(YIELD_COMMERCE)) < 0))
			{
				if (getNumCities() <= 3)
				{
					iBuildValue += 25 * countCityFeatures((FeatureTypes)iJ) * (3 - getNumCities() / 2);
				}
				else
				{
					iBuildValue += 25 * countCityFeatures((FeatureTypes)iJ);
				}
			}
			else if (getNumCities() <= 3)
			{
				iBuildValue += 5 * countCityFeatures((FeatureTypes)iJ) * (3 - getNumCities() / 2);
			}
			else
			{
				iBuildValue += 5 * countCityFeatures((FeatureTypes)iJ);
			}
		}
	}
	iValue += iBuildValue;

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tBuild value: %d", iBuildValue);
	}

	// does tech reveal bonus resources
	int iBonusRevealValue = 0;

	for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
	{
		if (GC.getBonusInfo((BonusTypes)iJ).getTechReveal() == eTech)
		{
			int iRevealValue = 150;
			iRevealValue += (AI_bonusVal((BonusTypes)iJ) * 50);

			BonusClassTypes eBonusClass = (BonusClassTypes)GC.getBonusInfo((BonusTypes)iJ).getBonusClassType();
			int iBonusClassTotal = (m_bonusClassRevealed[eBonusClass] + m_bonusClassUnrevealed[eBonusClass]);

			//iMultiplier is basically a desperation value
			//it gets larger as the AI runs out of options
			//Copper after failing to get horses is +66%
			//Iron after failing to get horse or copper is +200%
			//but with either copper or horse, Iron is only +25%
			int iMultiplier = 0;
			if (iBonusClassTotal > 0)
			{
				iMultiplier = (m_bonusClassRevealed[eBonusClass] - m_bonusClassHave[eBonusClass]);
				iMultiplier *= 100;
				iMultiplier /= iBonusClassTotal;

				iMultiplier *= (m_bonusClassRevealed[eBonusClass] + 1);
				iMultiplier /= ((m_bonusClassHave[eBonusClass] * iBonusClassTotal) + 1);
			}

			iMultiplier *= std::min(3, getNumCities());
			iMultiplier /= 3;

			iRevealValue *= 100 + iMultiplier;
			iRevealValue /= 100;

			// K-Mod
			// If we don't yet have the 'enable' tech, reduce the value of the reveal.
			if (GC.getBonusInfo((BonusTypes)iJ).getTechCityTrade() != eTech && !kTeam.isHasTech((TechTypes)(GC.getBonusInfo((BonusTypes)iJ).getTechCityTrade())))
				iRevealValue /= 3;
			// K-Mod end

			iBonusRevealValue += iRevealValue;
		}
		// K-Mod: Value for enabling resources that are already revealed
		else if (GC.getBonusInfo((BonusTypes)iJ).getTechCityTrade() == eTech && kTeam.isHasTech((TechTypes)(GC.getBonusInfo((BonusTypes)iJ).getTechReveal())))
		{
			int iOwned = countOwnedBonuses((BonusTypes)iJ);
			if (iOwned > 0)
			{
				int iEnableValue = 150;
				iEnableValue += (AI_bonusVal((BonusTypes)iJ) * 50);
				iEnableValue *= (iOwned > 1) ? 150 : 100;
				iEnableValue /= 100;

				iValue += iEnableValue;
			}
		}
		// K-Mod end
	}

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tBonus reveal value: %d", iBonusRevealValue);
	}

	iValue += iBonusRevealValue;

	/* ------------------ Unit Value  ------------------ */
	bool bEnablesUnitWonder;
	int iUnitValue = AI_techUnitValue(eTech, iPathLength, bEnablesUnitWonder);

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tUnit value: %d", iUnitValue);
	}

	iValue += iUnitValue;

	if (bEnablesUnitWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(400, "AI Research Wonder Unit ASYNC") : GC.getGame().getSorenRandNum(400, "AI Research Wonder Unit"));
		iValue += iWonderRandom + (bCapitalAlone ? 200 : 0);

		iRandomMax += 400;
	}


	/* ------------------ Building Value  ------------------ */
	bool bEnablesWonder;
	int iBuildingValue = AI_techBuildingValue(eTech, iPathLength, bEnablesWonder);

	if (gPlayerLogLevel > 2)
	{
		logBBAI("\tBuilding value: %d",
				iBuildingValue);
	}

	iValue += iBuildingValue;


	// if it gives at least one wonder
	if (bEnablesWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(800, "AI Research Wonder Building ASYNC") : GC.getGame().getSorenRandNum(800, "AI Research Wonder Building"));
		iValue += (500 + iWonderRandom) / (bAdvancedStart ? 5 : 1);

		iRandomMax += 800;
	}

	/* ------------------ Project Value  ------------------ */
	bool bEnablesProjectWonder = false;
	for (int iJ = 0; iJ < GC.getNumProjectInfos(); iJ++)
	{
		if (GC.getProjectInfo((ProjectTypes)iJ).getTechPrereq() == eTech)
		{
			iValue += 1000;

			if ((VictoryTypes)GC.getProjectInfo((ProjectTypes)iJ).getVictoryPrereq() != NO_VICTORY)
			{
				if (!GC.getProjectInfo((ProjectTypes)iJ).isSpaceship())
				{
					// Apollo
					iValue += (AI_isDoVictoryStrategy(AI_VICTORY_SPACE2) ? 2000 : 100);
				}
				// Space ship parts
				else if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
				{
					iValue += 1000;
				}
			}

			if (iPathLength <= 1 && getTotalPopulation() > 5 && isWorldProject((ProjectTypes)iJ)
			&& !GC.getGame().isProjectMaxedOut((ProjectTypes)iJ))
			{
				bEnablesProjectWonder = true;

				if (bCapitalAlone)
				{
					iValue += 100;
				}
			}
		}
	}
	if (bEnablesProjectWonder)
	{
		int iWonderRandom = ((bAsync) ? GC.getASyncRand().get(200, "AI Research Wonder Project ASYNC") : GC.getGame().getSorenRandNum(200, "AI Research Wonder Project"));
		iValue += iWonderRandom;

		iRandomMax += 200;
	}


	/* ------------------ Process Value  ------------------ */
	bool bIsGoodProcess = false;
	for (int iJ = 0; iJ < GC.getNumProcessInfos(); iJ++)
	{
		if (GC.getProcessInfo((ProcessTypes)iJ).getTechPrereq() == eTech)
		{
			iValue += 100;

			for (int iK = 0; iK < NUM_COMMERCE_TYPES; iK++)
			{
				iTempValue = (GC.getProcessInfo((ProcessTypes)iJ).getProductionToCommerceModifier(iK) * 4);

				iTempValue *= AI_commerceWeight((CommerceTypes)iK);
				iTempValue /= 100;

				if (iK == COMMERCE_GOLD || iK == COMMERCE_RESEARCH)
				{
					bIsGoodProcess = true;
				}
				else if ((iK == COMMERCE_CULTURE) && AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
				{
					iTempValue *= 3;
				}
				iValue += iTempValue;
			}
		}
	}

	if (bIsGoodProcess && bFinancialTrouble)
	{
		bool bHaveGoodProcess = false;
		for (int iJ = 0; iJ < GC.getNumProcessInfos(); iJ++)
		{
			if (kTeam.isHasTech(GC.getProcessInfo((ProcessTypes)iJ).getTechPrereq()))
			{
				bHaveGoodProcess = (GC.getProcessInfo((ProcessTypes)iJ).getProductionToCommerceModifier(COMMERCE_GOLD) + GC.getProcessInfo((ProcessTypes)iJ).getProductionToCommerceModifier(COMMERCE_RESEARCH)) > 0;
				if (bHaveGoodProcess)
				{
					break;
				}
			}
		}
		if (!bHaveGoodProcess)
		{
			iValue += 1500;
		}
	}

	/* ------------------ Civic Value  ------------------ */
	for (int iJ = 0; iJ < GC.getNumCivicInfos(); iJ++)
	{
		if (GC.getCivicInfo((CivicTypes)iJ).getTechPrereq() == eTech)
		{
			iValue += 200;

			const CivicTypes eCivic = getCivics((CivicOptionTypes)(GC.getCivicInfo((CivicTypes)iJ).getCivicOptionType()));
			if (NO_CIVIC != eCivic)
			{
				const int iCurrentCivicValue = AI_civicValue(eCivic);
				const int iNewCivicValue = AI_civicValue((CivicTypes)iJ);
				int iTechCivicValue = 0;

				if (iNewCivicValue > iCurrentCivicValue)
				{
					//	Because civic values can be negative theer is no absolute scale so we cannot meaningfully scale
					//	this relative to the current value.  Aslo 2400 is not enough to matter for some critical changes
					//iValue += std::min(2400, (2400 * (iNewCivicValue - iCurrentCivicValue)) / std::max(1, iCurrentCivicValue));
					iTechCivicValue = std::min(50000, 50 * (iNewCivicValue - iCurrentCivicValue));
					iValue += iTechCivicValue;
				}

				if (eCivic == GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic())
				{
					iValue += 600;
				}

				if (gPlayerLogLevel > 2)
				{
					logBBAI("Player %d - tech %S enabled civic %S with value %d vs current %d - additional tech value %d",
							getID(),
							kTech.getDescription(),
							GC.getCivicInfo((CivicTypes)iJ).getDescription(),
							iNewCivicValue,
							iCurrentCivicValue,
							iTechCivicValue);
				}
			}
		}
	}

	if (iPathLength <= 10)
	{
		if (GC.getGame().countKnownTechNumTeams(eTech) == 0)
		{
			int iReligionValue = 0;
			int iPotentialReligions = 0;
			for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
			{
				const TechTypes eReligionTech = GC.getReligionInfo((ReligionTypes)iJ).getTechPrereq();

				if (kTeam.isHasTech(eReligionTech) && !GC.getGame().isReligionSlotTaken((ReligionTypes)iJ) && canFoundReligion())
				{
					iPotentialReligions++;
				}
				if (eReligionTech == eTech)
				{
					if (!GC.getGame().isReligionSlotTaken((ReligionTypes)iJ))
					{
						int iRoll = 10000;
						if (!GC.getGame().isOption(GAMEOPTION_RELIGION_PICK) && canFoundReligion())
						{
							const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
							if (eFavorite != NO_RELIGION)
							{
								if (iJ == eFavorite)
								{
									iReligionValue += 1 + ((bAsync) ? GC.getASyncRand().get(1200, "AI Research Religion (Favorite) ASYNC") : GC.getGame().getSorenRandNum(1200, "AI Research Religion (Favorite)"));
								}
								else
								{
									iRoll *= 2;
									iRoll /= 3;
								}
							}
						}
						iReligionValue += 1 + ((bAsync) ? GC.getASyncRand().get(iRoll, "AI Research Religion ASYNC") : GC.getGame().getSorenRandNum(iRoll, "AI Research Religion"));

						if (iPathLength < 2)
						{
							iReligionValue *= 3;
							iReligionValue /= 2;
						}
					}
				}
			}

			if (iReligionValue > 0)
			{
				if (countHolyCities() < 1)
				{
					iReligionValue *= 2;
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
				{
					iReligionValue += 1000;
				}
				else
				{
					iReligionValue /= (1 + countHolyCities() + ((iPotentialReligions > 0) ? 1 : 0));
				}

				if ((countTotalHasReligion() == 0) && (iPotentialReligions == 0))
				{
					iReligionValue *= 2;
					iReligionValue += 500;
				}

				if (AI_isDoStrategy(AI_STRATEGY_DAGGER))
				{
					iReligionValue /= 2;
				}

				iReligionValue = (5 * iReligionValue / (iPathLength + 4));

				if (gPlayerLogLevel > 2)
				{
					logBBAI("\tReligion value: %d",
							iReligionValue);
				}

				iValue += iReligionValue;
			}

			int iCorporationValue = 0;
			for (int iJ = 0; iJ < GC.getNumCorporationInfos(); iJ++)
			{
				if (GC.getCorporationInfo((CorporationTypes)iJ).getTechPrereq() == eTech)
				{
					if (!(GC.getGame().isCorporationFounded((CorporationTypes)iJ)))
					{
						iCorporationValue += 100 + ((bAsync) ? GC.getASyncRand().get(2400, "AI Research Corporation ASYNC") : GC.getGame().getSorenRandNum(2400, "AI Research Corporation"));
					}
				}
			}

			if (iCorporationValue > 0)
			{
				iCorporationValue = (5 * iCorporationValue / (iPathLength + 4));

				if (gPlayerLogLevel > 2)
				{
					logBBAI("\tCorporation found value: %d",
							iCorporationValue);
				}

				iValue += iCorporationValue;
			}

			int iFreeStuffValue = 0;

			if ((UnitTypes)kTech.getFirstFreeUnit() != NO_UNIT)
			{
				int iGreatPeopleRandom = ((bAsync) ? GC.getASyncRand().get(3200, "AI Research Great People ASYNC") : GC.getGame().getSorenRandNum(3200, "AI Research Great People"));
				iFreeStuffValue += iGreatPeopleRandom;

				iRandomMax += 3200;

				if (bCapitalAlone)
				{
					iFreeStuffValue += 400;
				}

				iFreeStuffValue += 200;
			}

			//	Free techs are REALLY valuable - as an estimate we assume they are at least as valuable as the enabling tech
			//	since we'll be able to choose anything accessible once it is researched and up to twice as much as that on a
			//	random scale
			if (kTech.getFirstFreeTechs() > 0)
			{
				int	iPercentageMultiplier = kTech.getFirstFreeTechs() * ((bCapitalAlone ? 150 : 100) + (bAsync ? GC.getASyncRand().get(100, "AI Research Free Tech ASYNC") : GC.getGame().getSorenRandNum(100, "AI Research Free Tech")));

				iFreeStuffValue += (iValue * iPercentageMultiplier) / 100;
			}

			if (iFreeStuffValue > 0)
			{
				iFreeStuffValue = (5 * iFreeStuffValue / (iPathLength + 4));

				if (gPlayerLogLevel > 2)
				{
					logBBAI("\tFree on first discovery value: %d",
							iFreeStuffValue);
				}

				iValue += iFreeStuffValue;
			}
		}
	}

	iValue += kTech.getAIWeight();

	if (!isHumanPlayer() && iValue > 0)
	{
		for (int iJ = 0; iJ < GC.getNumFlavorTypes(); iJ++)
		{
			iValue += (AI_getFlavorValue((FlavorTypes)iJ) * kTech.getFlavorValue(iJ) * 20);
		}
	}

	if (kTech.isRepeat())
	{
		iValue /= 10;
	}

	if (gPlayerLogLevel > 2)
	{
		logBBAI("Player %d (%S) raw value for tech %S is %d",
				getID(),
				getCivilizationDescription(0),
				kTech.getDescription(),
				iValue);
	}

	if (!bIgnoreCost)
	{
		iValue *= (1 + (getResearchTurnsLeft((eTech), false)));
		iValue /= 10;
	}

	//Tech Whore
	if (!GC.getGame().isOption(GAMEOPTION_NO_TECH_TRADING) && (kTech.isTechTrading() || kTeam.isTechTrading()))
	{
		if ((bAsync ? GC.getASyncRand().get(100, "AI Tech Whore ASYNC") : GC.getGame().getSorenRandNum(100, "AI Tech Whore")) < (GC.getGame().isOption(GAMEOPTION_NO_TECH_BROKERING) ? 20 : 10))
		{
			int iKnownCount = 0;
			int iPossibleKnownCount = 0;

			for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
			{
				if (GET_TEAM((TeamTypes)iTeam).isAlive())
				{
					if (kTeam.isHasMet((TeamTypes)iTeam) && GET_TEAM((TeamTypes)iTeam).isHasTech(eTech))
					{
						iKnownCount++;
					}
					iPossibleKnownCount++;
				}
			}

			if (iKnownCount == 0 && iPossibleKnownCount > 2)
			{
				// Trade value
				iValue *= 100 + std::min(150, 25 * (iPossibleKnownCount - 2));
				iValue /= 100;
			}
		}
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		int iCVValue = AI_cultureVictoryTechValue(eTech);
		iValue *= (iCVValue + 10);
		iValue /= ((iCVValue < 100) ? 400 : 100);
	}

	int iRandomFactor = ((bAsync) ? GC.getASyncRand().get(20, "AI Research ASYNC") : GC.getGame().getSorenRandNum(20, "AI Research"));
	iValue += (iValue * (iRandomFactor - 10)) / 100;

	iValue = std::max(1, iValue);

	return iValue;
}

int CvPlayerAI::AI_techBuildingValue(TechTypes eTech, int iPathLength, bool& bEnablesWonder)
{
	PROFILE_FUNC();

	if (gPlayerLogLevel > 2)
	{
		logBBAI(
			"Player %d (%S) evaluating buildings for tech %S",
			getID(), getCivilizationDescription(0), GC.getTechInfo(eTech).getDescription()
		);
	}
#ifdef USE_BOTH_TECHBUILDING_EVALUATIONS
	bool bCapitalAlone = (GC.getGame().getElapsedGameTurns() > 0) ? AI_isCapitalAreaAlone() : false;
	bool bFinancialTrouble = AI_isFinancialTrouble();
	int iTeamCityCount = GET_TEAM(getTeam()).getNumCities();
	int iCoastalCities = countNumCoastalCities();
	int iCityCount = getNumCities();

	int iTempValue = 0;

	int iBestLandBuildingValue = 0;
	bool bIsCultureBuilding = false;
#endif
	int iValue = 0;
	int iExistingCultureBuildingCount = 0;

	bEnablesWonder = false;

	for (int iJ = 0; iJ < GC.getNumBuildingInfos(); iJ++)
	{
		const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iJ);

		if (GC.getGame().canEverConstruct(eLoopBuilding))
		{
			const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);
			if (isTechRequiredForBuilding(eTech, eLoopBuilding))
			{
				if (isWorldWonder(eLoopBuilding))
				{
					if (!GC.getGame().isBuildingMaxedOut(eLoopBuilding))
					{
						bEnablesWonder = true;
					}
				}

#ifdef USE_BOTH_TECHBUILDING_EVALUATIONS
				int iBuildingValue = 0;

				if (kLoopBuilding.getSpecialBuilding() != NO_SPECIALBUILDING)
				{
					iBuildingValue += ((bCapitalAlone) ? 100 : 25);
				}
				else
				{
					iBuildingValue += ((bCapitalAlone) ? 200 : 50);
				}

				//the granary effect is SO powerful it deserves special code
				if (kLoopBuilding.getFoodKept() > 0)
				{
					iBuildingValue += (15 * kLoopBuilding.getFoodKept());
				}

				if (kLoopBuilding.getAirlift() > 0)
				{
					iValue += 300;
				}

				if (kLoopBuilding.getMaintenanceModifier() < 0)
				{
					int iCount = 0;
					iTempValue = 0;
					foreach_(const CvCity * pLoopCity, cities())
					{
						iTempValue += pLoopCity->getMaintenanceTimes100();
						iCount++;
						if (iCount > 4)
						{
							break;
						}
					}
					iTempValue /= std::max(1, iCount);
					iTempValue *= -kLoopBuilding.getMaintenanceModifier();
					iTempValue /= 10 * 100;

					iValue += iTempValue;
				}

				iBuildingValue += 100;

				//	If the building has an AI weight assume that's for a good reason and factor it in here, discounting
				//	it somwhat if its a special build
				iBuildingValue += kLoopBuilding.getAIWeight() / (kLoopBuilding.getProductionCost() == -1 ? 5 : 1);

				if (!isLimitedWonder(eLoopBuilding) && kLoopBuilding.getCommerceChange(COMMERCE_CULTURE) > 0)
				{
					bIsCultureBuilding = true;
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
				{
					const int iMultiplier = (isLimitedWonder(eLoopBuilding) ? 1 : 3);
					iBuildingValue += 150 * kLoopBuilding.getCommerceChange(COMMERCE_CULTURE) * iMultiplier;
					iBuildingValue += kLoopBuilding.getCommerceModifier(COMMERCE_CULTURE) * 4 * iMultiplier;
				}

				if (bFinancialTrouble)
				{
					iBuildingValue -= kLoopBuilding.getMaintenanceModifier() * 15;
					iBuildingValue += kLoopBuilding.getYieldModifier(YIELD_COMMERCE) * 8;
					iBuildingValue += kLoopBuilding.getCommerceModifier(COMMERCE_GOLD) * 15;
				}

				// if this is a religious building, its not as useful
				const ReligionTypes eReligion = (ReligionTypes)kLoopBuilding.getReligionType();
				if (eReligion != NO_RELIGION)
				{

					// reduce by a factor based on how many cities we have with that relgion
					if (iTeamCityCount > 0)
					{
						const int iCitiesWithReligion = GET_TEAM(getTeam()).getHasReligionCount(eReligion);

						iBuildingValue *= (4 + iCitiesWithReligion);
						iBuildingValue /= (4 + iTeamCityCount);
					}

					// if this building requires a religion, then only count it as 1/7th as much
					// or in other words, only count things like temples once, not 7 times
					// doing it this way in case some mods give buildings to only one religion
					iBuildingValue /= std::max(1, GC.getNumReligionInfos());
				}

				// if we're close to pop domination, we love medicine!
				// don't adjust for negative modifiers to prevent ignoring assembly line, etc.
				if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3) && kLoopBuilding.getHealth() > 0)
				{
					iBuildingValue += kLoopBuilding.getHealth() * 150;
				}

				if (iPathLength <= 1 && kLoopBuilding.getPrereqAndTech() == eTech
				&& getTotalPopulation() > 5 && isWorldWonder(eLoopBuilding)
				&& !GC.getGame().isBuildingMaxedOut(eLoopBuilding))
				{
					bEnablesWonder = true;

					if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1)
					&&
						(
							kLoopBuilding.getCommerceChange(COMMERCE_CULTURE) >= 3
							||
							kLoopBuilding.getCommerceModifier(COMMERCE_CULTURE) >= 10
							)
					) iValue += 400;

					if (bCapitalAlone)
					{
						iBuildingValue += 400;
					}
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_DIPLOMACY1))
				{
					if (kLoopBuilding.getVoteSourceType() >= 0)
					{
						iValue += 400;
					}
				}

				if (iBuildingValue > iBestLandBuildingValue)
				{
					iBestLandBuildingValue = iBuildingValue;
				}

				// if water building, weight by coastal cities
				if (kLoopBuilding.isWater())
				{
					iBuildingValue *= iCoastalCities;
					iBuildingValue /= std::max(1, iCityCount / 2);
				}
				// if land building, is it the best?
				else if (iBuildingValue > iBestLandBuildingValue)
				{
					iBestLandBuildingValue = iBuildingValue;
				}

				if (gPlayerLogLevel > 2)
				{
					logBBAI("\tBuilding %S old mechanism value: %d",
							kLoopBuilding.getDescription(),
							iBuildingValue);
				}
#endif
				//	Now recalculate the new way
				int iBuildingValue = 0;
				int iEconomyFlags =
					(
						BUILDINGFOCUS_PRODUCTION |
						BUILDINGFOCUS_FOOD |
						BUILDINGFOCUS_GOLD |
						BUILDINGFOCUS_RESEARCH |
						BUILDINGFOCUS_MAINTENANCE |
						BUILDINGFOCUS_HAPPY |
						BUILDINGFOCUS_HEALTHY |
						BUILDINGFOCUS_CULTURE |
						BUILDINGFOCUS_SPECIALIST |
						BUILDINGFOCUS_DEFENSE |
						BUILDINGFOCUS_EXPERIENCE |
						BUILDINGFOCUS_MAINTENANCE |
						BUILDINGFOCUS_WORLDWONDER |
						BUILDINGFOCUS_DOMAINSEA |
						BUILDINGFOCUS_ESPIONAGE
					);
				CvCity* pRepresentativeCity = NULL;
				int	iBestValue = 0;
				int iTotalValue = 0;
				int iTotalWeight = 0;
				int iRepresentativeBuildingValueInCity = -1;
				bool bCanConstructCityFound = false;

				foreach_(CvCity * pLoopCity, cities())
				{
					int iWillGetProbability;

					if (pRepresentativeCity == NULL)
					{
						pRepresentativeCity = pLoopCity;
					}

					if (pLoopCity->canConstruct(eLoopBuilding, false, false, true, false, false, eTech, &iWillGetProbability))
					{
						iWillGetProbability = 100;

						if (!bCanConstructCityFound)
						{
							bCanConstructCityFound = true;
							pRepresentativeCity = pLoopCity;
						}
					}

					iTotalWeight += iWillGetProbability;
				}

				if (iTotalWeight > 0)
				{
					//	Chances are we'll be doing evaluations for many buildings and tending to use the same representative
					//	cities, so get the canTrain cache fully populated so that cached values can be used in the evaluation
					pRepresentativeCity->populateCanTrainCache(false);

					//	2/3rds factor applied here since the representative city we are using is likely to be large (early id)
					//	and so over-represent a bit
					iRepresentativeBuildingValueInCity = (2 * BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER * pRepresentativeCity->AI_buildingValue(eLoopBuilding, iEconomyFlags, true)) / 3;
				}

				iBestValue = iRepresentativeBuildingValueInCity;
				iTotalValue = (iRepresentativeBuildingValueInCity * iTotalWeight) / 100;

				if (isWorldWonder(eLoopBuilding))
				{
					if (!GC.getGame().isBuildingMaxedOut(eLoopBuilding))
					{
						iBuildingValue += (3 * iBestValue) / 2;	//	Opportunity cost for denying others
					}
				}
				else if (isTeamWonder(eLoopBuilding) || isNationalWonder(eLoopBuilding))
				{
					iBuildingValue += iBestValue;
				}
				else iBuildingValue += iTotalValue;

				//	Average value per city
				iBuildingValue /= std::max(1, getNumCities());

				if (gPlayerLogLevel > 2)
				{
					logBBAI("\tBuilding %S new mechanism value: %d", kLoopBuilding.getDescription(), iBuildingValue);
				}

				iValue += iBuildingValue;
			}
			else if (canConstruct(eLoopBuilding))
			{
				if (!isLimitedWonder(eLoopBuilding)
				&& (kLoopBuilding.getCommerceChange(COMMERCE_CULTURE) > 0 || kLoopBuilding.getCommercePerPopChange(COMMERCE_CULTURE) > 0))
				{
					iExistingCultureBuildingCount++;
				}

				const int iNumExisting = algo::count_if(cities(), bind(&CvCity::hasBuilding, _1, eLoopBuilding));

				if (iNumExisting > 0)
				{
					int iTempValue = 0;

					foreach_(const TechCommerceArray & pair, kLoopBuilding.getTechCommerceChanges100())
					{
						if (eTech == pair.first)
						{
							for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
							{
								iTempValue += 4 * pair.second[iJ];
							}
							break;
						}
					}
					foreach_(const TechArray & pair, kLoopBuilding.getTechYieldChanges100())
					{
						if (eTech == pair.first)
						{
							for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
							{
								iTempValue += 4 * pair.second[iJ];
							}
							break;
						}
					}
					foreach_(const TechCommerceArray & pair, kLoopBuilding.getTechCommerceModifiers())
					{
						if (eTech == pair.first)
						{
							for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
							{
								const int iCommerceModifier = pair.second[iJ];
								if (iCommerceModifier != 0)
								{
									iTempValue += 4 * getCommerceRate((CommerceTypes)iJ) * iCommerceModifier / getNumCities();
								}
							}
							break;
						}
					}
					foreach_(const TechArray & pair, kLoopBuilding.getTechYieldModifiers())
					{
						if (eTech == pair.first)
						{
							for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
							{
								const int iYieldModifier = pair.second[iJ];
								if (iYieldModifier != 0)
								{
									iTempValue += 4 * calculateTotalYield((YieldTypes)iJ) * iYieldModifier / getNumCities();
								}
							}
							break;
						}
					}

					for (int iJ = 0; iJ < GC.getNumSpecialistInfos(); iJ++)
					{
						int iSpecialistChange = kLoopBuilding.getTechSpecialistChange(eTech, iJ);

						if (iSpecialistChange != 0)
						{
							iTempValue += 800 * iSpecialistChange;
						}
					}

					if (iTempValue != 0)
					{
						iTempValue = iTempValue * BUILDING_VALUE_TO_TECH_BUILDING_VALUE_MULTIPLIER / 100;

						if (gPlayerLogLevel > 2)
						{
							logBBAI(
								"\tBuilding %S is modified.  We have %d of them, total value: %d",
								kLoopBuilding.getDescription(), iNumExisting, iTempValue
							);
						}
						iValue += iTempValue;
					}
				}
			}
		}
	}
	return iValue;
}

// abstraction of the loop to check if civilization can already train a settler, and set a boolean if it can
bool CvPlayerAI::AI_canTrainSettler() {
	PROFILE_EXTRA_FUNC();
	if (!m_canTrainSettler) {
		for (int iI = GC.getNumUnitInfos() - 1; iI > -1; iI--)
		{
			if (GC.getUnitInfo((UnitTypes)iI).getDefaultUnitAIType() == UNITAI_SETTLE && canTrain((UnitTypes)iI))
			{
				m_canTrainSettler = true;
				break;
			}
		}
	}
	return m_canTrainSettler;
}


int CvPlayerAI::AI_techUnitValue(TechTypes eTech, int iPathLength, bool& bEnablesUnitWonder)
{
	PROFILE_EXTRA_FUNC();
	const bool bWarPlan =
		(
			GET_TEAM(getTeam()).hasWarPlan(true)
			|| // or aggressive personality
			GET_TEAM(getTeam()).AI_getTotalWarOddsTimes100() > 400
		);
	const bool bCapitalAlone = (GC.getGame().getElapsedGameTurns() > 0) ? AI_isCapitalAreaAlone() : false;
	const int iNumCities = getNumCities();
	const int iHasMetCount = GET_TEAM(getTeam()).getHasMetCivCount(true);
	const int iCoastalCities = countNumCoastalCities();
	const CvCity* pCapitalCity = getCapitalCity();

	bEnablesUnitWonder = false;
	int iValue = 0;
	for (int i = GC.getNumUnitInfos() - 1; i > -1; i--)
	{
		const UnitTypes eUnitX = static_cast<UnitTypes>(i);

		if (GC.getGame().canEverTrain(eUnitX) && isTechRequiredForUnit(eTech, eUnitX))
		{
			const CvUnitInfo& unitX = GC.getUnitInfo(eUnitX);
			iValue += 200;

			if (unitX.getPrereqAndTech() == eTech)
			{
				int iUnitValue = 0;
				int iNavalValue = 0;
				int iMilitaryValue = 0;

				// BBAI TODO: Change this to evaluating all unitai types defined in XML for unit?
				// Without this change many unit types are hard to evaluate, like offensive value of rifles
				// or defensive value of collateral siege
				switch (unitX.getDefaultUnitAIType())
				{
				case UNITAI_UNKNOWN:
				case UNITAI_ANIMAL:
				case UNITAI_SUBDUED_ANIMAL:
				case UNITAI_BARB_CRIMINAL:
					break;
				case UNITAI_HUNTER:
				case UNITAI_HUNTER_ESCORT:
				{
					iUnitValue += 200;
					break;
				}
				case UNITAI_SETTLE:
				{
					// FIRST settler unit has a much higher weighting
					iUnitValue += 10000;
					if (AI_canTrainSettler()) {
						iUnitValue -= 9000;
					}
					break;
				}
				case UNITAI_WORKER:
					iUnitValue += 800;
					break;

				case UNITAI_HEALER:
					iUnitValue += 200;
					iMilitaryValue += 200;

				case UNITAI_HEALER_SEA:
					iUnitValue += 200;
					iMilitaryValue += 200;

				case UNITAI_PROPERTY_CONTROL:
					iUnitValue += 400;
					iMilitaryValue += 50;

				case UNITAI_PROPERTY_CONTROL_SEA:
					iUnitValue += 350;
					iMilitaryValue += 50;

				case UNITAI_INVESTIGATOR:
					iUnitValue += 400;

				case UNITAI_INFILTRATOR:
					iUnitValue += 200;
					iMilitaryValue += 75;

				case UNITAI_ESCORT:
					iUnitValue += 100;
					iMilitaryValue += 400;

				case UNITAI_SEE_INVISIBLE:
					iUnitValue += 400;
					iMilitaryValue += 100;

				case UNITAI_ATTACK:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 800 : 0);
					iUnitValue += 100;
					break;

				case UNITAI_ATTACK_CITY:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 800 : 0);
					if (unitX.getBombardRate() > 0)
					{
						iMilitaryValue += 200;

						if (AI_calculateTotalBombard(DOMAIN_LAND) == 0)
						{
							iMilitaryValue += 800;
							if (AI_isDoStrategy(AI_STRATEGY_DAGGER))
							{
								iMilitaryValue += 1000;
							}
						}
					}
					iUnitValue += 100;
					break;

				case UNITAI_COLLATERAL:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					break;

				case UNITAI_PILLAGE:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_PILLAGE_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 300 : 200);
					break;

				case UNITAI_RESERVE:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					iMilitaryValue += (AI_isDoStrategy(AI_STRATEGY_DAGGER) ? 600 : 0);
					break;

				case UNITAI_PARADROP:
					iMilitaryValue += ((bWarPlan) ? 600 : 300);
					break;

				case UNITAI_CITY_DEFENSE:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					iMilitaryValue += ((!bCapitalAlone) ? 400 : 200);
					iUnitValue += ((iHasMetCount > 0) ? 800 : 200);
					break;

				case UNITAI_CITY_COUNTER:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					break;

				case UNITAI_CITY_SPECIAL:
					iMilitaryValue += ((bWarPlan) ? 800 : 400);
					break;

				case UNITAI_EXPLORE:
					iUnitValue += ((bCapitalAlone) ? 100 : 200);
					break;

				case UNITAI_MISSIONARY:
					iUnitValue += ((getStateReligion() != NO_RELIGION) ? 600 : 300);
					break;

				case UNITAI_PROPHET:
				case UNITAI_ARTIST:
				case UNITAI_SCIENTIST:
				case UNITAI_GENERAL:
				case UNITAI_GREAT_HUNTER:
				case UNITAI_GREAT_ADMIRAL:
				case UNITAI_MERCHANT:
				case UNITAI_ENGINEER:
				case UNITAI_WORKER_SEA:
					break;

				case UNITAI_SPY:
					iMilitaryValue += ((bWarPlan) ? 100 : 50);
					break;

				case UNITAI_ICBM:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				case UNITAI_SEE_INVISIBLE_SEA:
					// BBAI TODO: Boost value for maps where Barb ships are pestering us
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 400;
					}
					iNavalValue += 100;
					break;

				case UNITAI_ATTACK_SEA:
					// BBAI TODO: Boost value for maps where Barb ships are pestering us
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 200 : 100);
					}
					iNavalValue += 100;
					break;

				case UNITAI_RESERVE_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					iNavalValue += 100;
					break;

				case UNITAI_ESCORT_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					iNavalValue += 100;
					break;

				case UNITAI_EXPLORE_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += ((bCapitalAlone) ? 1800 : 600);
					}
					break;

				case UNITAI_ASSAULT_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan || bCapitalAlone) ? 400 : 200);
					}
					iNavalValue += 200;
					break;

				case UNITAI_SETTLER_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += ((bWarPlan || bCapitalAlone) ? 100 : 200);
					}
					iNavalValue += 200;
					break;

				case UNITAI_MISSIONARY_SEA:
					if (iCoastalCities > 0)
					{
						iUnitValue += 100;
					}
					break;

				case UNITAI_SPY_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 100;
					}
					break;

				case UNITAI_CARRIER_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					break;

				case UNITAI_MISSILE_CARRIER_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 100 : 50);
					}
					break;

				case UNITAI_PIRATE_SEA:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += 100;
					}
					iNavalValue += 100;
					break;

				case UNITAI_ATTACK_AIR:
					iMilitaryValue += ((bWarPlan) ? 1200 : 800);
					break;

				case UNITAI_DEFENSE_AIR:
					iMilitaryValue += ((bWarPlan) ? 1200 : 800);
					break;

				case UNITAI_CARRIER_AIR:
					if (iCoastalCities > 0)
					{
						iMilitaryValue += ((bWarPlan) ? 200 : 100);
					}
					iNavalValue += 400;
					break;

				case UNITAI_MISSILE_AIR:
					iMilitaryValue += ((bWarPlan) ? 200 : 100);
					break;

				default:
					FErrorMsg("error");
					break;
				}

				if (AI_isDoStrategy(AI_STRATEGY_ALERT1))
				{
					if (unitX.getUnitAIType(UNITAI_COLLATERAL))
					{
						iUnitValue += 500;
					}
					if (unitX.getUnitAIType(UNITAI_CITY_DEFENSE))
					{
						iUnitValue += 10 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (AI_isDoStrategy(AI_STRATEGY_TURTLE) && iPathLength <= 1)
				{
					if (unitX.getUnitAIType(UNITAI_COLLATERAL))
					{
						iUnitValue += 1000;
					}
					if (unitX.getUnitAIType(UNITAI_CITY_DEFENSE))
					{
						iUnitValue += 20 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
				{
					if (unitX.getUnitAIType(UNITAI_ATTACK_CITY))
					{
						iUnitValue += 15 * GC.getGame().AI_combatValue(eUnitX);
					}
				}
				else if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST1))
				{
					if (unitX.getUnitAIType(UNITAI_ATTACK_CITY))
					{
						iUnitValue += 5 * GC.getGame().AI_combatValue(eUnitX);
					}
				}

				if (unitX.getUnitAIType(UNITAI_ASSAULT_SEA) && iCoastalCities > 0)
				{
					int iAssaultValue = 0;
					UnitTypes eExistingUnit = NO_UNIT;
					if (AI_bestAreaUnitAIValue(UNITAI_ASSAULT_SEA, NULL, &eExistingUnit) == 0)
					{
						iAssaultValue += 250;
					}
					else if (eExistingUnit != NO_UNIT)
					{
						iAssaultValue += 1000 * std::max(0, AI_unitImpassableCount(eUnitX) - AI_unitImpassableCount(eExistingUnit));

						const int iOld = GC.getUnitInfo(eExistingUnit).getMoves() * GC.getUnitInfo(eExistingUnit).getCargoSpace();

						iAssaultValue += 800 * (unitX.getMoves() * unitX.getCargoSpace() - iOld) / std::max(1, iOld);
					}

					if (iAssaultValue > 0)
					{
						foreach_(CvArea * areaX, GC.getMap().areas())
						{
							if (AI_isPrimaryArea(areaX) && areaX->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
							{
								iAssaultValue *= 4; break;
							}
						}
						iUnitValue += iAssaultValue;
					}
				}

				if (iNavalValue > 0 && pCapitalCity != NULL)
				{
					iUnitValue += iNavalValue * 2 * (1 + iNumCities - pCapitalCity->area()->getCitiesPerPlayer(getID())) / iNumCities;
				}

				if (AI_totalUnitAIs(unitX.getDefaultUnitAIType()) == 0)
				{
					// do not give bonus to seagoing units if they are worthless
					if (iUnitValue > 0)
					{
						iUnitValue *= 2;
					}

					if (pCapitalCity != NULL && unitX.getDefaultUnitAIType() == UNITAI_EXPLORE)
					{
						iUnitValue += AI_neededExplorers(pCapitalCity->area()) * 400;
					}

					if (unitX.getDefaultUnitAIType() == UNITAI_EXPLORE_SEA)
					{
						iUnitValue += 400;
						iUnitValue += ((GC.getGame().countCivTeamsAlive() - iHasMetCount) * 200);
					}
				}

				if (pCapitalCity != NULL && unitX.getUnitAIType(UNITAI_SETTLER_SEA))
				{
					UnitTypes eExistingUnit = NO_UNIT;
					int iBestAreaValue = 0;
					AI_getNumAreaCitySites(pCapitalCity->getArea(), iBestAreaValue);

					//Early Expansion by sea
					if (AI_bestAreaUnitAIValue(UNITAI_SETTLER_SEA, NULL, &eExistingUnit) == 0)
					{
						CvArea* pWaterArea = pCapitalCity->waterArea();
						if (pWaterArea != NULL)
						{
							if (iBestAreaValue == 0)
							{
								iUnitValue += 2000;
							}
							else
							{
								int iBestOtherValue = 0;
								AI_getNumAdjacentAreaCitySites(pWaterArea->getID(), pCapitalCity->getArea(), iBestOtherValue);

								if (iBestAreaValue < iBestOtherValue)
								{
									iUnitValue += 1000;
								}
								else if (iBestOtherValue > 0)
								{
									iUnitValue += 500;
								}
							}
						}
					}
					// Landlocked expansion over ocean
					else if (eExistingUnit != NO_UNIT
					&& AI_unitImpassableCount(eUnitX) < AI_unitImpassableCount(eExistingUnit)
					&& iBestAreaValue < AI_getMinFoundValue())
					{
						iUnitValue += (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION2) ? 2000 : 500);
					}
				}

				if (iMilitaryValue > 0)
				{
					if (iHasMetCount == 0)
					{
						iMilitaryValue /= 2;
					}
					if (bCapitalAlone)
					{
						iMilitaryValue *= 2;
						iMilitaryValue /= 3;
					}
					// K-Mod
					if (AI_isDoStrategy(AI_STRATEGY_GET_BETTER_UNITS))
					{
						iMilitaryValue *= 3;
						iMilitaryValue /= 2;
					}
					iUnitValue += iMilitaryValue;
				}

				if (iPathLength <= 1 && getTotalPopulation() > 5 && isWorldUnit(eUnitX) && !GC.getGame().isUnitMaxedOut(eUnitX))
				{
					bEnablesUnitWonder = true;
				}
				iValue += iUnitValue;
			}
		}
	}
	return iValue;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

void CvPlayerAI::AI_chooseFreeTech()
{
	clearResearchQueue();

	const TechTypes eBestTech = AI_bestTech(1, true);

	if (eBestTech != NO_TECH)
	{
		GET_TEAM(getTeam()).setHasTech(eBestTech, true, getID(), true, true);
	}
}

void CvPlayerAI::AI_startGoldenAge()
{
	// Golden age start - reconsider civics at the first opportunity
	AI_setCivicTimer(0);
}

void CvPlayerAI::AI_chooseResearch()
{
	PROFILE_EXTRA_FUNC();
	FAssert(!isNPC())
		clearResearchQueue();

	if (getCurrentResearch() == NO_TECH && !isNPC())
	{
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (iI != getID() && GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam())
			&& GET_PLAYER((PlayerTypes)iI).getCurrentResearch() != NO_TECH
			&& canResearch(GET_PLAYER((PlayerTypes)iI).getCurrentResearch()))
			{
				pushResearch(GET_PLAYER((PlayerTypes)iI).getCurrentResearch());
			}
		}
	}

	if (getCurrentResearch() == NO_TECH)
	{
		const TechTypes eBestTech = AI_bestTech(isHumanPlayer() ? 1 : (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3) ? 1 : 3));

		if (eBestTech != NO_TECH)
		{
			CvTechInfo& tech = GC.getTechInfo(eBestTech);

			OutputDebugString(CvString::format("Game turn %d, AI chooses tech %S\n", GC.getGame().getGameTurn(), tech.getDescription()).c_str());
			pushResearch(eBestTech);
		}
	}
}


DiploCommentTypes CvPlayerAI::AI_getGreeting(PlayerTypes ePlayer) const
{
	TeamTypes eWorstEnemy;

	if (GET_PLAYER(ePlayer).getTeam() != getTeam())
	{
		eWorstEnemy = GET_TEAM(getTeam()).AI_getWorstEnemy();

		if ((eWorstEnemy != NO_TEAM) && (eWorstEnemy != GET_PLAYER(ePlayer).getTeam()) && GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isHasMet(eWorstEnemy) && (GC.getASyncRand().get(4) == 0))
		{
			if (GET_PLAYER(ePlayer).AI_hasTradedWithTeam(eWorstEnemy) && !atWar(GET_PLAYER(ePlayer).getTeam(), eWorstEnemy))
			{
				return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_WORST_ENEMY_TRADING");
			}
			else
			{
				return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_WORST_ENEMY");
			}
		}
		else if ((getNumNukeUnits() > 0) && (GC.getASyncRand().get(4) == 0))
		{
			return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_NUKES");
		}
		else if ((GET_PLAYER(ePlayer).getPower() < getPower()) && AI_getAttitude(ePlayer) < ATTITUDE_PLEASED && (GC.getASyncRand().get(4) == 0))
		{
			return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_UNIT_BRAG");
		}
	}

	return (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GREETINGS");
}


bool CvPlayerAI::AI_isWillingToTalk(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssertMsg(getPersonalityType() != NO_LEADER, "getPersonalityType() is not expected to be equal with NO_LEADER");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (GET_PLAYER(ePlayer).getTeam() == getTeam()
		|| GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam())
		|| GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return true;
	}

	if (GET_TEAM(getTeam()).isHuman())
	{
		return false;
	}
	bool bRuthlessAI = GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS);
	if (bRuthlessAI)
	{
		if (AI_getMemoryCount(ePlayer, MEMORY_BACKSTAB) > 0)
		{
			return false;
		}
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		int iRefuseDuration = (GC.getLeaderHeadInfo(getPersonalityType()).getRefuseToTalkWarThreshold() * ((GET_TEAM(getTeam()).AI_isChosenWar(GET_PLAYER(ePlayer).getTeam())) ? 2 : 1));

		int iOurSuccess = 1 + GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER(ePlayer).getTeam());
		int iTheirSuccess = 1 + GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWarSuccess(getTeam());
		if (iTheirSuccess > iOurSuccess * 2)
		{
			iRefuseDuration *= 20 + ((80 * iOurSuccess * 2) / iTheirSuccess);
			iRefuseDuration /= 100;
		}

		if (GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER(ePlayer).getTeam()) < iRefuseDuration)
		{
			return false;
		}

		if (GET_TEAM(getTeam()).isAVassal())
		{
			return false;
		}
		/************************************************************************************************/
		/* Afforess					  Start		 07/12/10											   */
		/*																							  */
		/*																							  */
		/************************************************************************************************/
		if (GET_PLAYER(ePlayer).getNumCities() == 0)
		{
			return false;
		}

		if (bRuthlessAI)
		{
			if (!AI_isFinancialTrouble())
			{
				if (iOurSuccess * 2 > iTheirSuccess * 3)
				{
					return false;
				}
			}
		}
		/************************************************************************************************/
		/* Afforess						 END															*/
		/************************************************************************************************/
	}
	else
	{
		if (AI_getMemoryCount(ePlayer, MEMORY_STOPPED_TRADING_RECENT) > 0)
		{
			return false;
		}
	}

	return true;
}


// XXX what if already at war???
// Returns true if the AI wants to sneak attack...
bool CvPlayerAI::AI_demandRebukedSneak(PlayerTypes ePlayer) const
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	FAssert(!(GET_TEAM(getTeam()).isAVassal()));
	FAssert(!(GET_TEAM(getTeam()).isHuman()));

	if (GC.getGame().getSorenRandNum(100, "AI Demand Rebuked") < GC.getLeaderHeadInfo(getPersonalityType()).getDemandRebukedSneakProb())
	{
		if (GET_TEAM(getTeam()).getPower(true) > GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePower(getTeam()))
		{
			return true;
		}
	}

	return false;
}


// XXX what if already at war???
// Returns true if the AI wants to declare war...
bool CvPlayerAI::AI_demandRebukedWar(PlayerTypes ePlayer) const
{
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	FAssert(!(GET_TEAM(getTeam()).isAVassal()));
	FAssert(!(GET_TEAM(getTeam()).isHuman()));

	// needs to be async because it only happens on the computer of the player who is in diplomacy...
	if (GC.getASyncRand().get(100, "AI Demand Rebuked ASYNC") < GC.getLeaderHeadInfo(getPersonalityType()).getDemandRebukedWarProb())
	{
		if (GET_TEAM(getTeam()).getPower(true) > GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePower())
		{
			if (GET_TEAM(getTeam()).AI_isAllyLandTarget(GET_PLAYER(ePlayer).getTeam()))
			{
				return true;
			}
		}
	}

	return false;
}


// XXX maybe make this a little looser (by time...)
bool CvPlayerAI::AI_hasTradedWithTeam(TeamTypes eTeam) const
{
	PROFILE_EXTRA_FUNC();
	int iI;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == eTeam)
			{
				if ((AI_getPeacetimeGrantValue((PlayerTypes)iI) + AI_getPeacetimeTradeValue((PlayerTypes)iI)) > 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// static // Toffer - Should perhaps be in CvGameCoreUtil?
AttitudeTypes CvPlayerAI::AI_getAttitudeFromValue(int iAttitudeVal)
{
	if (iAttitudeVal > 9)
	{
		return ATTITUDE_FRIENDLY;
	}
	if (iAttitudeVal > 2)
	{
		return ATTITUDE_PLEASED;
	}
	if (iAttitudeVal > -3)
	{
		return ATTITUDE_CAUTIOUS;
	}
	if (iAttitudeVal > -9)
	{
		return ATTITUDE_ANNOYED;
	}
	return ATTITUDE_FURIOUS;
}

AttitudeTypes CvPlayerAI::AI_getAttitude(PlayerTypes ePlayer, bool bForced) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (GET_PLAYER(ePlayer).isAlive())
	{
		return AI_getAttitudeFromValue(AI_getAttitudeVal(ePlayer, bForced));
	}
	FErrorMsg("ePlayer should ideally be alive");
	return NO_ATTITUDE;
}


int CvPlayerAI::AI_getAttitudeVal(PlayerTypes ePlayer, bool bForced) const
{
	PROFILE_FUNC();

	//	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	//AI Autoplay calls this
	if (isNPC() || GET_PLAYER(ePlayer).isNPC())
	{
		return -100;
	}
	if (bForced
	&& (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) && !GET_TEAM(getTeam()).isCapitulated()))
	{
		return 100;
	}

	if (m_aiAttitudeCache[ePlayer] != MAX_INT)
	{
		return m_aiAttitudeCache[ePlayer];
	}

	int iAttitude = GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttitude();

	iAttitude += GC.getHandicapInfo(GET_PLAYER(ePlayer).getHandicapType()).getAttitudeChange();

	iAttitude += GET_PLAYER(ePlayer).getAIAttitudeModifier();

	if (!GET_PLAYER(ePlayer).isHumanPlayer())
	{
		iAttitude += (4 - abs(AI_getPeaceWeight() - GET_PLAYER(ePlayer).AI_getPeaceWeight()));
		iAttitude += std::min(GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect(), GC.getLeaderHeadInfo(GET_PLAYER(ePlayer).getPersonalityType()).getWarmongerRespect());
	}

	iAttitude -= std::max(0, (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getNumMembers() - GET_TEAM(getTeam()).getNumMembers()));

	const int iRankDifference = (GC.getGame().getPlayerRank(getID()) - GC.getGame().getPlayerRank(ePlayer));

	if (iRankDifference > 0)
	{
		iAttitude += ((GC.getLeaderHeadInfo(getPersonalityType()).getWorseRankDifferenceAttitudeChange() * iRankDifference) / (GC.getGame().countCivPlayersEverAlive() + 1));
	}
	else
	{
		iAttitude += ((GC.getLeaderHeadInfo(getPersonalityType()).getBetterRankDifferenceAttitudeChange() * -(iRankDifference)) / (GC.getGame().countCivPlayersEverAlive() + 1));
	}

	if ((GC.getGame().getPlayerRank(getID()) >= (GC.getGame().countCivPlayersEverAlive() / 2)) &&
		  (GC.getGame().getPlayerRank(ePlayer) >= (GC.getGame().countCivPlayersEverAlive() / 2)))
	{
		iAttitude++;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWarSuccess(getTeam()) > GET_TEAM(getTeam()).AI_getWarSuccess(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getLostWarAttitudeChange();
	}

	iAttitude += AI_getTraitAttitude(ePlayer);
	iAttitude += AI_getCloseBordersAttitude(ePlayer);
	iAttitude += AI_getWarAttitude(ePlayer);
	iAttitude += AI_getPeaceAttitude(ePlayer);
	iAttitude += AI_getSameReligionAttitude(ePlayer);
	iAttitude += AI_getDifferentReligionAttitude(ePlayer);
	iAttitude += AI_getBonusTradeAttitude(ePlayer);
	iAttitude += AI_getOpenBordersAttitude(ePlayer);
	iAttitude += AI_getDefensivePactAttitude(ePlayer);
	iAttitude += AI_getRivalDefensivePactAttitude(ePlayer);
	iAttitude += AI_getRivalVassalAttitude(ePlayer);
	iAttitude += AI_getShareWarAttitude(ePlayer);
	iAttitude += AI_getFavoriteCivicAttitude(ePlayer);
	iAttitude += AI_getTradeAttitude(ePlayer);
	iAttitude += AI_getRivalTradeAttitude(ePlayer);
	iAttitude += AI_getCivicShareAttitude(ePlayer);
	iAttitude += AI_getEmbassyAttitude(ePlayer);
	iAttitude += AI_getCivicAttitudeChange(ePlayer);

	for (int iI = 0; iI < NUM_MEMORY_TYPES; iI++)
	{
		iAttitude += AI_getMemoryAttitude(ePlayer, ((MemoryTypes)iI));
	}

	iAttitude += AI_getColonyAttitude(ePlayer);
	iAttitude += AI_getAttitudeExtra(ePlayer);

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isRebelAgainst(getTeam()))
	{
		iAttitude -= 5;
	}
	else if (GET_TEAM(getTeam()).isRebelAgainst(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude -= 3;
	}

	if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
	{
		if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getWorstEnemy() == GET_TEAM(getTeam()).AI_getWorstEnemy())
		{
			iAttitude += 2;
		}
	}

	m_aiAttitudeCache[ePlayer] = range(iAttitude, -100, 100);

	return range(iAttitude, -100, 100);
}


// BEGIN: Show Hidden Attitude Mod 01/22/2009
bool isShowPersonalityModifiers()
{
	return true;
}

bool isShowSpoilerModifiers()
{
	return true;
}

int CvPlayerAI::AI_getFirstImpressionAttitude(PlayerTypes ePlayer) const
{
	bool bShowPersonalityAttitude = isShowPersonalityModifiers();
	CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
	int iAttitude = GC.getHandicapInfo(kPlayer.getHandicapType()).getAttitudeChange();

	//ls612: If you Start as Minors the first impression is not important
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_START_AS_MINORS))
	{
		return 0;
	}

	if (bShowPersonalityAttitude)
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttitude();
		if (!kPlayer.isHumanPlayer())
		{
			if (isShowSpoilerModifiers())
			{
				// iBasePeaceWeight + iPeaceWeightRand
				iAttitude += (4 - abs(AI_getPeaceWeight() - kPlayer.AI_getPeaceWeight()));
			}
			else
			{
				// iBasePeaceWeight
				iAttitude += (4 - abs(GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() - GC.getLeaderHeadInfo(kPlayer.getPersonalityType()).getBasePeaceWeight()));
			}
			iAttitude += std::min(GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect(), GC.getLeaderHeadInfo(kPlayer.getPersonalityType()).getWarmongerRespect());
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getTeamSizeAttitude(PlayerTypes ePlayer) const
{
	return -std::max(0, (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getNumMembers() - GET_TEAM(getTeam()).getNumMembers()));
}


// Count only players visible on the active player's scoreboard
int CvPlayerAI::AI_getKnownPlayerRank(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	PlayerTypes eActivePlayer = GC.getGame().getActivePlayer();
	if (NO_PLAYER == eActivePlayer || GC.getGame().isDebugMode()) {
		// Use the full scoreboard
		return GC.getGame().getPlayerRank(ePlayer);
	}

	TeamTypes eActiveTeam = GC.getGame().getActiveTeam();
	int iRank = 0;
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		PlayerTypes eRankPlayer = GC.getGame().getRankPlayer(iI);
		if (eRankPlayer != NO_PLAYER)
		{
			CvTeam& kRankTeam = GET_TEAM(GET_PLAYER(eRankPlayer).getTeam());
			if (kRankTeam.isAlive() && (kRankTeam.isHasMet(eActiveTeam) || kRankTeam.isHuman()))
			{
				if (eRankPlayer == ePlayer) {
					return iRank;
				}
				iRank++;
			}
		}
	}

	// Should only get here if we tried to find the rank of an unknown player
	return iRank + 1;
}

int CvPlayerAI::AI_getTraitAttitude(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	CvPlayerAI& kPlayer = GET_PLAYER(ePlayer);
	int iAttitude = kPlayer.getAIAttitudeModifier();

	if (iAttitude < 0)
	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			TraitTypes eTrait = ((TraitTypes)iI);
			if (GC.getTraitInfo(eTrait).getAttitudeModifier() < 0 && kPlayer.hasTrait(eTrait) && GET_PLAYER(getID()).hasTrait(eTrait))
			{
				iAttitude *= -1;
			}
		}
	}
	return iAttitude;
}

int CvPlayerAI::AI_getBetterRankDifferenceAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	int iRankDifference;
	if (isShowSpoilerModifiers())
	{
		iRankDifference = GC.getGame().getPlayerRank(ePlayer) - GC.getGame().getPlayerRank(getID());
	}
	else
	{
		iRankDifference = AI_getKnownPlayerRank(ePlayer) - AI_getKnownPlayerRank(getID());
	}

	if (iRankDifference > 0)
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getBetterRankDifferenceAttitudeChange() * iRankDifference / (GC.getGame().countCivPlayersEverAlive() + 1);
	}

	return 0;
}


int CvPlayerAI::AI_getWorseRankDifferenceAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	int iRankDifference;
	if (isShowSpoilerModifiers())
	{
		iRankDifference = GC.getGame().getPlayerRank(getID()) - GC.getGame().getPlayerRank(ePlayer);
	}
	else
	{
		iRankDifference = AI_getKnownPlayerRank(getID()) - AI_getKnownPlayerRank(ePlayer);
	}

	if (iRankDifference > 0)
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getWorseRankDifferenceAttitudeChange() * iRankDifference / (GC.getGame().countCivPlayersEverAlive() + 1);
	}

	return 0;
}


int CvPlayerAI::AI_getLowRankAttitude(PlayerTypes ePlayer) const
{
	int iThisPlayerRank;
	int iPlayerRank;
	if (isShowSpoilerModifiers())
	{
		iThisPlayerRank = GC.getGame().getPlayerRank(getID());
		iPlayerRank = GC.getGame().getPlayerRank(ePlayer);
	}
	else
	{
		iThisPlayerRank = AI_getKnownPlayerRank(getID());
		iPlayerRank = AI_getKnownPlayerRank(ePlayer);
	}

	int iMedianRank = GC.getGame().countCivPlayersEverAlive() / 2;
	return (iThisPlayerRank >= iMedianRank && iPlayerRank >= iMedianRank) ? 1 : 0;
}


int CvPlayerAI::AI_getLostWarAttitude(PlayerTypes ePlayer) const
{
	if (!isShowPersonalityModifiers())
	{
		return 0;
	}

	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();
	if (!isShowSpoilerModifiers() && NO_PLAYER != GC.getGame().getActivePlayer())
	{
		// Hide war success for wars you are not involved in
		if (GC.getGame().getActiveTeam() != getTeam() && GC.getGame().getActiveTeam() != eTeam)
		{
			return 0;
		}
	}

	if (GET_TEAM(eTeam).AI_getWarSuccess(getTeam()) > GET_TEAM(getTeam()).AI_getWarSuccess(eTeam))
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getLostWarAttitudeChange();
	}

	return 0;
}
// END: Show Hidden Attitude Mod


int CvPlayerAI::AI_calculateStolenCityRadiusPlots(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssert(ePlayer != getID());

	int iCount = 0;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		const CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);

		if (pLoopPlot->getOwner() == ePlayer && pLoopPlot->isPlayerCityRadius(getID()))
		{
			iCount++;
		}
	}

	return iCount;
}


int CvPlayerAI::AI_getCloseBordersAttitude(PlayerTypes ePlayer) const
{
	if (m_aiCloseBordersAttitudeCache[ePlayer] == MAX_INT)
	{
		PROFILE_FUNC();
		int iPercent;

		if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
		{
			return 0;
		}

		iPercent = std::min(60, (AI_calculateStolenCityRadiusPlots(ePlayer) * 3));

		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					  06/12/10								jdog5000	  */
		/*																							  */
		/* Bugfix, Victory Strategy AI																  */
		/************************************************************************************************/
		if (GET_TEAM(getTeam()).AI_isLandTarget(GET_PLAYER(ePlayer).getTeam(), true))
		{
			iPercent += 40;
		}

		if (AI_isDoStrategy(AI_VICTORY_CONQUEST3))
		{
			iPercent = std::min(120, (3 * iPercent) / 2);
		}
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/

		m_aiCloseBordersAttitudeCache[ePlayer] = ((GC.getLeaderHeadInfo(getPersonalityType()).getCloseBordersAttitudeChange() * iPercent) / 100);
	}

	return m_aiCloseBordersAttitudeCache[ePlayer];
}


int CvPlayerAI::AI_getWarAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude -= 3;
	}

	if (GC.getLeaderHeadInfo(getPersonalityType()).getAtWarAttitudeDivisor() != 0)
	{
		int iAttitudeChange = (GET_TEAM(getTeam()).AI_getAtWarCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAtWarAttitudeDivisor());
		iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAtWarAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAtWarAttitudeChangeLimit()));
	}

	return iAttitude;
}


int CvPlayerAI::AI_getPeaceAttitude(PlayerTypes ePlayer) const
{
	if (GC.getLeaderHeadInfo(getPersonalityType()).getAtPeaceAttitudeDivisor() != 0)
	{
		int iAttitudeChange = (GET_TEAM(getTeam()).AI_getAtPeaceCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getAtPeaceAttitudeDivisor());
		return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getAtPeaceAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getAtPeaceAttitudeChangeLimit()));
	}

	return 0;
}


int CvPlayerAI::AI_getSameReligionAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if ((getStateReligion() != NO_RELIGION) && (getStateReligion() == GET_PLAYER(ePlayer).getStateReligion()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getSameReligionAttitudeChange();

		if (hasHolyCity(getStateReligion()))
		{
			iAttitude++;
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getSameReligionAttitudeDivisor() != 0)
		{
			int iAttitudeChange = (AI_getSameReligionCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getSameReligionAttitudeDivisor());
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getSameReligionAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getSameReligionAttitudeChangeLimit()));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getDifferentReligionAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if ((getStateReligion() != NO_RELIGION) && (GET_PLAYER(ePlayer).getStateReligion() != NO_RELIGION) && (getStateReligion() != GET_PLAYER(ePlayer).getStateReligion()))
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getDifferentReligionAttitudeChange();

		if (hasHolyCity(getStateReligion()))
		{
			iAttitude--;
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getDifferentReligionAttitudeDivisor() != 0)
		{
			int iAttitudeChange = (AI_getDifferentReligionCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getDifferentReligionAttitudeDivisor());
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getDifferentReligionAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getDifferentReligionAttitudeChangeLimit()));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getBonusTradeAttitude(PlayerTypes ePlayer) const
{
	int iAttitudeChange;

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getBonusTradeAttitudeDivisor() != 0)
		{
			iAttitudeChange = (AI_getBonusTradeCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getBonusTradeAttitudeDivisor());
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getBonusTradeAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getBonusTradeAttitudeChangeLimit()));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getOpenBordersAttitude(PlayerTypes ePlayer) const
{
	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getOpenBordersAttitudeDivisor() != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getOpenBordersCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getOpenBordersAttitudeDivisor());
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getOpenBordersAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getOpenBordersAttitudeChangeLimit()));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getDefensivePactAttitude(PlayerTypes ePlayer) const
{
	if (getTeam() != GET_PLAYER(ePlayer).getTeam() && (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam())))
	{
		return GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactAttitudeChangeLimit();
	}

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactAttitudeDivisor() != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getDefensivePactCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactAttitudeDivisor());
			return range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactAttitudeChangeLimit()));
		}
	}

	return 0;
}


int CvPlayerAI::AI_getRivalDefensivePactAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		return iAttitude;
	}

	if (!(GET_TEAM(getTeam()).isDefensivePact(GET_PLAYER(ePlayer).getTeam())))
	{
		iAttitude -= ((4 * GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getDefensivePactCount(GET_PLAYER(ePlayer).getTeam())) / std::max(1, (GC.getGame().countCivTeamsAlive() - 2)));
	}

	return iAttitude;
}


int CvPlayerAI::AI_getRivalVassalAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getTeam() == GET_PLAYER(ePlayer).getTeam() || GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		return iAttitude;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getVassalCount(getTeam()) > 0)
	{
		iAttitude -= (6 * GET_TEAM(GET_PLAYER(ePlayer).getTeam()).getPower(true)) / std::max(1, GC.getGame().countTotalCivPower());
	}

	return iAttitude;
}


int CvPlayerAI::AI_getShareWarAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (!atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		if (GET_TEAM(getTeam()).AI_shareWar(GET_PLAYER(ePlayer).getTeam()))
		{
			iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getShareWarAttitudeChange();
		}

		if (GC.getLeaderHeadInfo(getPersonalityType()).getShareWarAttitudeDivisor() != 0)
		{
			int iAttitudeChange = (GET_TEAM(getTeam()).AI_getShareWarCounter(GET_PLAYER(ePlayer).getTeam()) / GC.getLeaderHeadInfo(getPersonalityType()).getShareWarAttitudeDivisor());
			iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getShareWarAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getShareWarAttitudeChangeLimit()));
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getFavoriteCivicAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() != NO_CIVIC)
	{
		if (isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()) && GET_PLAYER(ePlayer).isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()))
		{
			iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivicAttitudeChange();

			if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivicAttitudeDivisor() != 0)
			{
				int iAttitudeChange = (AI_getFavoriteCivicCounter(ePlayer) / GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivicAttitudeDivisor());
				iAttitude += range(iAttitudeChange, -(abs(GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivicAttitudeChangeLimit())), abs(GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivicAttitudeChangeLimit()));
			}
		}
	}

	return iAttitude;
}


int CvPlayerAI::AI_getTradeAttitude(PlayerTypes ePlayer) const
{
	// XXX human only?
	return range(((AI_getPeacetimeGrantValue(ePlayer) + std::max(0, (AI_getPeacetimeTradeValue(ePlayer) - GET_PLAYER(ePlayer).AI_getPeacetimeTradeValue(getID())))) / ((GET_TEAM(getTeam()).AI_getHasMetCounter(GET_PLAYER(ePlayer).getTeam()) + 1) * 5)), 0, 4);
}


int CvPlayerAI::AI_getRivalTradeAttitude(PlayerTypes ePlayer) const
{
	// XXX human only?

	return -(range(((GET_TEAM(getTeam()).AI_getEnemyPeacetimeGrantValue(GET_PLAYER(ePlayer).getTeam()) + (GET_TEAM(getTeam()).AI_getEnemyPeacetimeTradeValue(GET_PLAYER(ePlayer).getTeam()) / 3)) / ((GET_TEAM(getTeam()).AI_getHasMetCounter(GET_PLAYER(ePlayer).getTeam()) + 1) * 10)), 0, 4));
}


int CvPlayerAI::AI_getMemoryAttitude(PlayerTypes ePlayer, MemoryTypes eMemory) const
{
	return ((AI_getMemoryCount(ePlayer, eMemory) * GC.getLeaderHeadInfo(getPersonalityType()).getMemoryAttitudePercent(eMemory)) / 100);
}

int CvPlayerAI::AI_getColonyAttitude(PlayerTypes ePlayer) const
{
	int iAttitude = 0;

	if (getParent() == ePlayer)
	{
		iAttitude += GC.getLeaderHeadInfo(getPersonalityType()).getFreedomAppreciation();
	}

	return iAttitude;
}



PlayerVoteTypes CvPlayerAI::AI_diploVote(const VoteSelectionSubData& kVoteData, VoteSourceTypes eVoteSource, bool bPropose)
{
	PROFILE_FUNC();

	const VoteTypes eVote = kVoteData.eVote;

	if (GC.getGame().isTeamVote(eVote))
	{
		if (GC.getGame().isTeamVoteEligible(getTeam(), eVoteSource))
		{
			return (PlayerVoteTypes)getTeam();
		}
		int iBestValue;

		if (GC.getVoteInfo(eVote).isVictory())
		{
			iBestValue = 7;
		}
		else iBestValue = 0;

		PlayerVoteTypes eBestTeam = PLAYER_VOTE_ABSTAIN;

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if (GET_TEAM((TeamTypes)iI).isAlive()
			&& GC.getGame().isTeamVoteEligible((TeamTypes)iI, eVoteSource))
			{
				if (GET_TEAM(getTeam()).isVassal((TeamTypes)iI))
				{
					return (PlayerVoteTypes)iI;
				}

				const int iValue = GET_TEAM(getTeam()).AI_getAttitudeVal((TeamTypes)iI);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestTeam = (PlayerVoteTypes)iI;
				}
			}
		}
		return eBestTeam;
	}

	TeamTypes eSecretaryGeneral = GC.getGame().getSecretaryGeneral(eVoteSource);

	// Remove blanket auto approval for friendly secretary
	bool bFriendlyToSecretary = false;
	if (!bPropose && eSecretaryGeneral != NO_TEAM)
	{
		if (eSecretaryGeneral == getTeam())
		{
			return PLAYER_VOTE_YES;
		}
		bFriendlyToSecretary = (GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) == ATTITUDE_FRIENDLY);
	}

	bool bDefy = false;
	bool bValid = true;

	for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
	{
		const CivicTypes eCivic = (CivicTypes)iI;

		if (GC.getVoteInfo(eVote).isForceCivic(iI) && !isCivic(eCivic))
		{
			const CivicTypes eBestCivic = AI_bestCivic((CivicOptionTypes)GC.getCivicInfo(eCivic).getCivicOptionType());

			if (eBestCivic != NO_CIVIC && eBestCivic != eCivic)
			{
				const int iBestCivicValue = AI_civicValue(eBestCivic);
				const int iNewCivicValue =
					(
						bFriendlyToSecretary
						?
						AI_civicValue(eCivic) * 6 / 5
						:
						AI_civicValue(eCivic)
					);

				if (iBestCivicValue > iNewCivicValue * 120 / 100)
				{
					bValid = false;

					// Increase odds of defiance, particularly on AggressiveAI
					if (iBestCivicValue > iNewCivicValue * (140 + GC.getGame().getSorenRandNum(GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 50 : 80, "AI Erratic Defiance (Force Civic)")) / 100)
					{
						bDefy = true;
					}
					break;
				}
			}
		}
	}

	if (bValid && GC.getVoteInfo(eVote).getTradeRoutes() > 0)
	{
		if (bFriendlyToSecretary)
		{
			return PLAYER_VOTE_YES;
		}

		if (getNumCities() > GC.getGame().getNumCities() * 2 / (1 + GC.getGame().countCivPlayersAlive()))
		{
			bValid = false;
		}
	}

	if (bValid && GC.getVoteInfo(eVote).isNoNukes())
	{
		int iVoteBanThreshold = GET_TEAM(getTeam()).getNukeInterception() / 3;
		iVoteBanThreshold += GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb();
		iVoteBanThreshold *= std::max(1, GC.getLeaderHeadInfo(getPersonalityType()).getWarmongerRespect());

		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iVoteBanThreshold *= 2;
		}

		bool bAnyHasSdi = false;
		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if (GET_TEAM((TeamTypes)iI).isAlive() && iI != getTeam()
			&& GET_TEAM((TeamTypes)iI).getNukeInterception() > 0)
			{
				bAnyHasSdi = true;
				break;
			}
		}

		if (!bAnyHasSdi && GET_TEAM(getTeam()).getNukeInterception() > 0 && GET_TEAM(getTeam()).getNumNukeUnits() > 0)
		{
			iVoteBanThreshold *= 2;
		}

		if (bFriendlyToSecretary)
		{
			iVoteBanThreshold *= 2;
			iVoteBanThreshold /= 3;
		}

		bValid = (GC.getGame().getSorenRandNum(100, "AI nuke ban vote") > iVoteBanThreshold);

		if (AI_isDoStrategy(AI_STRATEGY_OWABWNW))
		{
			bValid = false;
		}
		else if (
			GET_TEAM(getTeam()).getNumNukeUnits() / std::max(1, GET_TEAM(getTeam()).getNumMembers())
			<
			GC.getGame().countTotalNukeUnits() / std::max(1, GC.getGame().countCivPlayersAlive()))
		{
			bValid = false;
		}


		if (!bValid && AI_getNumTrainAIUnits(UNITAI_ICBM) > 0
		&& GC.getGame().getSorenRandNum(AI_isDoStrategy(AI_STRATEGY_OWABWNW) ? 2 : 3, "AI Erratic Defiance (No Nukes)") == 0)
		{
			bDefy = true;
		}
	}

	if (bValid && GC.getVoteInfo(eVote).isFreeTrade())
	{
		if (bFriendlyToSecretary)
		{
			return PLAYER_VOTE_YES;
		}
		int iOpenCount = 0;
		int iClosedCount = 0;

		for (int iI = 0; iI < MAX_TEAMS; iI++)
		{
			if (GET_TEAM((TeamTypes)iI).isAlive() && iI != getTeam())
			{
				if (GET_TEAM(getTeam()).isOpenBorders((TeamTypes)iI))
				{
					iOpenCount += GET_TEAM((TeamTypes)iI).getNumCities();
				}
				else iClosedCount += GET_TEAM((TeamTypes)iI).getNumCities();
			}
		}

		if (iOpenCount >= getNumCities() * getTradeRoutes())
		{
			bValid = false;
		}

		if (iClosedCount == 0)
		{
			bValid = false;
		}
	}

	if (bValid)
	{
		if (GC.getVoteInfo(eVote).isOpenBorders())
		{
			if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}
			bValid = true;

			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (iI != getTeam() && GET_TEAM((TeamTypes)iI).isVotingMember(eVoteSource)
				&& NO_DENIAL != GET_TEAM(getTeam()).AI_openBordersTrade((TeamTypes)iI))
				{
					bValid = false;
					break;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).isDefensivePact())
		{
			if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}

			bValid = true;

			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (iI != getTeam() && GET_TEAM((TeamTypes)iI).isVotingMember(eVoteSource)
				&& NO_DENIAL != GET_TEAM(getTeam()).AI_defensivePactTrade((TeamTypes)iI))
				{
					bValid = false;
					break;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).isForcePeace())
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes ePeaceTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			int iWarsWinning = 0;
			int iWarsLosing = 0;
			int iChosenWar = 0;

			bool bLosingBig = false;
			bool bWinningBig = false;
			bool bThisPlayerWinning = false;

			int iWinDeltaThreshold = 3 * GC.getWAR_SUCCESS_ATTACKING();
			int iLossAbsThreshold = std::max(3, getNumMilitaryUnits() / 40) * GC.getWAR_SUCCESS_ATTACKING();

			bool bAggressiveAI = GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE);
			if (bAggressiveAI)
			{
				iWinDeltaThreshold *= 2;
				iWinDeltaThreshold /= 3;

				iLossAbsThreshold *= 4;
				iLossAbsThreshold /= 3;
			}

			// Is ePeaceTeam winning wars?
			for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
			{
				if (GET_TEAM((TeamTypes)iI).isAlive() && iI != ePeaceTeam
				&& GET_TEAM((TeamTypes)iI).isAtWar(ePeaceTeam))
				{
					const int iPeaceTeamSuccess = GET_TEAM(ePeaceTeam).AI_getWarSuccess((TeamTypes)iI);
					const int iOtherTeamSuccess = GET_TEAM((TeamTypes)iI).AI_getWarSuccess(ePeaceTeam);

					if (iPeaceTeamSuccess - iOtherTeamSuccess > iWinDeltaThreshold)
					{
						// Have to be ahead by at least a few victories to count as win
						++iWarsWinning;

						if (iPeaceTeamSuccess - iOtherTeamSuccess > 3 * iWinDeltaThreshold)
						{
							bWinningBig = true;
						}
					}
					else if (iOtherTeamSuccess >= iPeaceTeamSuccess)
					{
						if (iI == getTeam() && iOtherTeamSuccess - iPeaceTeamSuccess > iWinDeltaThreshold)
						{
							bThisPlayerWinning = true;
						}

						if (iOtherTeamSuccess > iLossAbsThreshold)
						{
							// Have to have non-trivial loses
							++iWarsLosing;

							if (iOtherTeamSuccess - iPeaceTeamSuccess > 3 * iLossAbsThreshold)
							{
								bLosingBig = true;
							}
						}
						else if (GET_TEAM(ePeaceTeam).AI_getAtWarCounter((TeamTypes)iI) < 10
						// Not winning, just recently attacked, and in multiple wars, be pessimistic
						// Counts ties from no actual battles)
						&& GET_TEAM(ePeaceTeam).getAtWarCount(true) > 1
						&& !GET_TEAM(ePeaceTeam).AI_isChosenWar((TeamTypes)iI))
						{
							++iWarsLosing;
						}
					}

					if (GET_TEAM(ePeaceTeam).AI_isChosenWar((TeamTypes)iI))
					{
						++iChosenWar;
					}
				}
			}

			if (ePeaceTeam == getTeam())
			{
				const int iPeaceRand =
					(
						GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight()
						/
						(
							(bAggressiveAI ? 2 : 1)
							*
							(AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST2) ? 2 : 1) // K-Mod
							)
					);

				// Always true for real war-mongers, rarely true for less aggressive types
				const bool bWarmongerRoll = 0 == GC.getGame().getSorenRandNum(iPeaceRand, "AI Erratic Defiance (Force Peace)");

				if (bLosingBig && (!bWarmongerRoll || bPropose))
				{
					// Non-warmongers want peace to escape loss
					bValid = true;
				}
				else if (!bLosingBig && (iChosenWar > iWarsLosing || AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))) // K-Mod
				{
					// If chosen to be in most wars, keep it going
					bValid = false;
				}
				else
				{
					// If losing most wars, vote for peace
					bValid = (iWarsLosing > iWarsWinning);
				}

				if (!bValid && !bLosingBig && bWinningBig && bWarmongerRoll && !AI_isFinancialTrouble())
				{
					bDefy = true;
				}
			}
			else if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (GET_TEAM(ePeaceTeam).isAtWar(getTeam()))
			{
				// Do we want to end this war?
				bValid =
					(
						GET_TEAM(getTeam()).AI_endWarVal(ePeaceTeam) > 3 * GET_TEAM(ePeaceTeam).AI_endWarVal(getTeam()) / 2
						&&
						(bWinningBig || iWarsWinning > iWarsLosing || GET_TEAM(getTeam()).getAtWarCount(true, true) > 1)
					);

				// Do we want to defy the peace resolution?
				if (!bValid && bThisPlayerWinning && iWarsLosing >= iWarsWinning && !bPropose
				&& !GET_TEAM(getTeam()).isAVassal()
				&& (GET_TEAM(getTeam()).getAtWarCount(true) == 1 || bLosingBig)
				// Can we continue this war with defiance penalties?
				&& !AI_isFinancialTrouble()
				&& (
					GC.getGame().getSorenRandNum
					(
						GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight() / (bAggressiveAI ? 2 : 1),
						"AI Erratic Defiance (Force Peace)"
					) == 0
					)) bDefy = true;


				if (!bValid && !bDefy && !bPropose
				&& GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) > GC.getLeaderHeadInfo(getPersonalityType()).getVassalRefuseAttitudeThreshold())
				{
					// Influence by secretary
					if (NO_DENIAL == GET_TEAM(getTeam()).AI_makePeaceTrade(ePeaceTeam, eSecretaryGeneral))
					{
						bValid = true;
					}
					else if (eSecretaryGeneral != NO_TEAM && GET_TEAM(getTeam()).isVassal(eSecretaryGeneral))
					{
						bValid = true;
					}
				}
			}
			else
			{
				if (GET_TEAM(getTeam()).AI_getWarPlan(ePeaceTeam) != NO_WARPLAN)
				{
					// Keep planned enemy occupied
					bValid = false;
				}
				else if (GET_TEAM(getTeam()).AI_shareWar(ePeaceTeam) && !GET_TEAM(getTeam()).isVassal(ePeaceTeam))
				{
					// Keep ePeaceTeam at war with our common enemies
					bValid = false;
				}
				else if (iWarsLosing > iWarsWinning)
				{
					// Feel pity for team that is losing (if like them enough to not declare war on them)
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(ePeaceTeam) >= GC.getLeaderHeadInfo(getPersonalityType()).getDeclareWarThemRefuseAttitudeThreshold());
				}
				else
				{
					// Stop a team that is winning (if don't like them enough to join them in war)
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(ePeaceTeam) < GC.getLeaderHeadInfo(getPersonalityType()).getDeclareWarRefuseAttitudeThreshold());
				}

				if (!bValid && bFriendlyToSecretary && !GET_TEAM(getTeam()).isVassal(ePeaceTeam))
				{
					// Influence by secretary
					bValid = true;
				}
			}
		}
		else if (GC.getVoteInfo(eVote).isForceNoTrade())
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes eEmbargoTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (eEmbargoTeam == getTeam())
			{
				bValid = false;
				if (!isNoForeignTrade())
				{
					bDefy = true;
				}
			}
			else if (bFriendlyToSecretary)
			{
				return PLAYER_VOTE_YES;
			}
			else if (canStopTradingWithTeam(eEmbargoTeam))
			{
				bValid = (NO_DENIAL == AI_stopTradingTrade(eEmbargoTeam, kVoteData.ePlayer));
				if (bValid)
				{
					bValid = (GET_TEAM(getTeam()).AI_getAttitude(eEmbargoTeam) <= ATTITUDE_CAUTIOUS);
				}
			}
			else
			{
				bValid = (GET_TEAM(getTeam()).AI_getAttitude(eEmbargoTeam) < ATTITUDE_CAUTIOUS);
			}
		}
		else if (GC.getVoteInfo(eVote).isForceWar())
		{
			FAssert(kVoteData.ePlayer != NO_PLAYER);
			TeamTypes eWarTeam = GET_PLAYER(kVoteData.ePlayer).getTeam();

			if (eSecretaryGeneral == getTeam() && !bPropose)
			{
				bValid = true;
			}
			else if (eWarTeam == getTeam() || GET_TEAM(getTeam()).isVassal(eWarTeam))
			{
				// Explicit rejection by all who will definitely be attacked
				bValid = false;
			}
			else if (GET_TEAM(getTeam()).AI_getWarPlan(eWarTeam) != NO_WARPLAN)
			{
				bValid = true;
			}
			else
			{
				if (!bPropose && GET_TEAM(getTeam()).isAVassal())
				{
					// Vassals always deny war trade requests and thus previously always voted no
					bValid = false;

					if
					(
						!GET_TEAM(getTeam()).hasWarPlan(true)
						&&
						(
							eSecretaryGeneral == NO_TEAM
							||
							GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral)
							>
							GC.getLeaderHeadInfo(getPersonalityType()).getDeclareWarRefuseAttitudeThreshold()
						)
					)
					{
						if (eSecretaryGeneral != NO_TEAM && GET_TEAM(getTeam()).isVassal(eSecretaryGeneral))
						{
							bValid = true;
						}
						else if
						(
							(
								GET_TEAM(getTeam()).isAVassal()
								?
								GET_TEAM(getTeam()).getCurrentMasterPower(true)
								:
								GET_TEAM(getTeam()).getPower(true)
							)
							> GET_TEAM(eWarTeam).getDefensivePower()
						)
						{
							bValid = true;
						}
					}
				}
				else
				{
					bValid = (bPropose || NO_DENIAL == GET_TEAM(getTeam()).AI_declareWarTrade(eWarTeam, eSecretaryGeneral));
				}

				if (bValid)
				{
					int iNoWarOdds = GC.getLeaderHeadInfo(getPersonalityType()).getNoWarAttitudeProb((GET_TEAM(getTeam()).AI_getAttitude(eWarTeam)));
					bValid = ((iNoWarOdds < 30) || (GC.getGame().getSorenRandNum(100, "AI War Vote Attitude Check (Force War)") > iNoWarOdds));
				}
				/*
				else
				{
					// Consider defying resolution
					if( !GET_TEAM(getTeam()).isAVassal() )
					{
						if( eSecretaryGeneral == NO_TEAM || GET_TEAM(getTeam()).AI_getAttitude(eWarTeam) > GET_TEAM(getTeam()).AI_getAttitude(eSecretaryGeneral) )
						{
							if( GET_TEAM(getTeam()).AI_getAttitude(eWarTeam) > GC.getLeaderHeadInfo(getPersonalityType()).getDefensivePactRefuseAttitudeThreshold() )
							{
								int iDefyRand = GC.getLeaderHeadInfo(getPersonalityType()).getBasePeaceWeight();
								iDefyRand /= (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 2 : 1);

								if (GC.getGame().getSorenRandNum(iDefyRand, "AI Erratic Defiance (Force War)") > 0)
								{
									bDefy = true;
								}
							}
						}
					}
				}
				*/
			}
		}
		else if (GC.getVoteInfo(eVote).isAssignCity())
		{
			bValid = false;

			FAssert(kVoteData.ePlayer != NO_PLAYER);
			CvPlayer& kPlayer = GET_PLAYER(kVoteData.ePlayer);
			CvCity* pCity = kPlayer.getCity(kVoteData.iCityId);
			if (NULL != pCity && NO_PLAYER != kVoteData.eOtherPlayer && kVoteData.eOtherPlayer != pCity->getOwner())
			{
				if (!bPropose && eSecretaryGeneral == getTeam() || GET_PLAYER(kVoteData.eOtherPlayer).getTeam() == getTeam())
				{
					bValid = true;
				}
				else if (kPlayer.getTeam() == getTeam())
				{
					bValid = false;
					// BBAI TODO: Wonders, holy city, aggressive AI?
					if (GC.getGame().getSorenRandNum(3, "AI Erratic Defiance (Assign City)") == 0)
					{
						bDefy = true;
					}
				}
				else
				{
					bValid = (AI_getAttitude(kVoteData.ePlayer) < AI_getAttitude(kVoteData.eOtherPlayer));
				}
			}
		}
	}

	// Don't defy resolutions from friends
	if (bDefy && !bFriendlyToSecretary && canDefyResolution(eVoteSource, kVoteData))
	{
		return PLAYER_VOTE_NEVER;
	}
	return (bValid ? PLAYER_VOTE_YES : PLAYER_VOTE_NO);
}


int CvPlayerAI::AI_dealVal(PlayerTypes ePlayer, const CLinkList<TradeData>* pList, bool bIgnoreAnnual, int iChange) const
{
	PROFILE_EXTRA_FUNC();
	CLLNode<TradeData>* pNode;

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	int iValue = 0;

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		iValue += GET_TEAM(getTeam()).AI_endWarVal(GET_PLAYER(ePlayer).getTeam());
	}

	for (pNode = pList->head(); pNode; pNode = pList->next(pNode))
	{
		FAssert(!pNode->m_data.m_bHidden);

		switch (pNode->m_data.m_eItemType)
		{
			case TRADE_TECHNOLOGIES:
			{
				iValue += GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_RESOURCES:
			{
				if (!bIgnoreAnnual)
				{
					iValue += AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, iChange);

					// The partner player is also loosing value for it, which is good for us
					iValue += GET_PLAYER(ePlayer).AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), getID(), -iChange);
				}
				break;
			}
			case TRADE_CITIES:
			{
				CvCity* pCity = GET_PLAYER(ePlayer).getCity(pNode->m_data.m_iData);
				if (pCity != NULL)
				{
					iValue += AI_cityTradeVal(pCity);

					//	The partner player is also loosing value for it, which is good for us
					iValue += GET_PLAYER(ePlayer).AI_ourCityValue(pCity);
				}
				break;
			}
			case TRADE_GOLD:
			{
				iValue += AI_getGoldValue(pNode->m_data.m_iData, AI_goldTradeValuePercent());
				break;
			}
			case TRADE_GOLD_PER_TURN:
			{
				if (!bIgnoreAnnual)
				{
					iValue += AI_getGoldValue(pNode->m_data.m_iData * getTreatyLength(), AI_goldTradeValuePercent());
				}
				break;
			}
			case TRADE_MAPS:
			{
				iValue += GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_SURRENDER:
			{
				if (!bIgnoreAnnual)
				{
					iValue += GET_TEAM(getTeam()).AI_surrenderTradeVal(GET_PLAYER(ePlayer).getTeam());
				}
				break;
			}
			case TRADE_VASSAL:
			{
				if (!bIgnoreAnnual)
				{
					iValue += GET_TEAM(getTeam()).AI_vassalTradeVal(GET_PLAYER(ePlayer).getTeam());
				}
				break;
			}
			case TRADE_OPEN_BORDERS:
			{
				iValue += GET_TEAM(getTeam()).AI_openBordersTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_DEFENSIVE_PACT:
			{
				iValue += GET_TEAM(getTeam()).AI_defensivePactTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_PEACE:
			{
				iValue += GET_TEAM(getTeam()).AI_makePeaceTradeVal(((TeamTypes)(pNode->m_data.m_iData)), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_WAR:
			{
				iValue += GET_TEAM(getTeam()).AI_declareWarTradeVal(((TeamTypes)(pNode->m_data.m_iData)), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_EMBARGO:
			{
				iValue += AI_stopTradingTradeVal(((TeamTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_CIVIC:
			{
				iValue += AI_civicTradeVal(((CivicTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_RELIGION:
			{
				iValue += AI_religionTradeVal(((ReligionTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_EMBASSY:
			{
				iValue += GET_TEAM(getTeam()).AI_embassyTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_CONTACT:
			{
				iValue += GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_CORPORATION:
			{
				iValue += AI_corporationTradeVal((CorporationTypes)pNode->m_data.m_iData);

				//	Partner is losing it also
				iValue += GET_PLAYER(ePlayer).AI_corporationTradeVal((CorporationTypes)pNode->m_data.m_iData);
				break;
			}
			case TRADE_PLEDGE_VOTE:
			{
				iValue += AI_pledgeVoteTradeVal(GC.getGame().getVoteTriggered(GC.getGame().getCurrentVoteID()), ((PlayerVoteTypes)(pNode->m_data.m_iData)), ePlayer);
				break;
			}
			case TRADE_SECRETARY_GENERAL_VOTE:
			{
				iValue += AI_secretaryGeneralTradeVal((VoteSourceTypes)(pNode->m_data.m_iData), ePlayer);
				break;
			}
			case TRADE_RITE_OF_PASSAGE:
			{
				iValue += GET_TEAM(getTeam()).AI_LimitedBordersTradeVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_FREE_TRADE_ZONE:
			{
				iValue += GET_TEAM(getTeam()).AI_FreeTradeAgreementVal(GET_PLAYER(ePlayer).getTeam());
				break;
			}
			case TRADE_WORKER:
			{
				const CvUnit* pUnit = GET_PLAYER(ePlayer).getUnit(pNode->m_data.m_iData);
				if (pUnit)
				{
					iValue += AI_workerTradeVal(pUnit);
				}
				break;
			}
			case TRADE_MILITARY_UNIT:
			{
				const CvUnit* pUnit = GET_PLAYER(ePlayer).getUnit(pNode->m_data.m_iData);
				if (pUnit)
				{
					iValue += AI_militaryUnitTradeVal(pUnit);
				}
				break;
			}
		}
	}
	return iValue;
}


bool CvPlayerAI::AI_goldDeal(const CLinkList<TradeData>* pList) const
{
	PROFILE_EXTRA_FUNC();
	CLLNode<TradeData>* pNode;

	for (pNode = pList->head(); pNode; pNode = pList->next(pNode))
	{
		FAssert(!(pNode->m_data.m_bHidden));

		switch (pNode->m_data.m_eItemType)
		{
		case TRADE_GOLD:
		case TRADE_GOLD_PER_TURN:
			return true;
			break;
		}
	}

	return false;
}


/// \brief AI decision making on a proposal it is given
///
/// In this function the AI considers whether or not to accept another player's proposal.  This is used when
/// considering proposals from the human player made in the diplomacy window as well as a couple other places.
bool CvPlayerAI::AI_considerOffer(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, int iChange) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (AI_goldDeal(pTheirList) && AI_goldDeal(pOurList))
	{
		return false;
	}
	CLLNode<TradeData>* pNode;

	if (iChange > -1)
	{
		for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
		{
			if (getTradeDenial(ePlayer, pNode->m_data) != NO_DENIAL)
			{
				return false;
			}
		}
	}
	const CvPlayerAI& dealer = GET_PLAYER(ePlayer);

	if (dealer.getTeam() == getTeam())
	{
		return true;
	}

	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CORPORATION && pTheirList->getLength() == 0)
		{
			return false;
		}
	}
	const CvTeamAI& myTeam = GET_TEAM(getTeam());

	// Don't always accept giving deals, TRADE_VASSAL and TRADE_SURRENDER come with strings attached
	bool bVassalTrade = false;
	for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_VASSAL)
		{
			bVassalTrade = true;

			for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
			{
				if (GET_TEAM((TeamTypes)iTeam).isAlive() && iTeam != getTeam() && iTeam != dealer.getTeam()
				&& atWar(dealer.getTeam(), (TeamTypes)iTeam) && !atWar(getTeam(), (TeamTypes)iTeam)
				&& myTeam.AI_declareWarTrade((TeamTypes)iTeam, dealer.getTeam(), false) != NO_DENIAL)
				{
					return false;
				}
			}
		}
		else if (pNode->m_data.m_eItemType == TRADE_SURRENDER)
		{
			bVassalTrade = true;

			if (!myTeam.AI_acceptSurrender(dealer.getTeam()))
			{
				return false;
			}
		}
	}
	if (!bVassalTrade && pOurList->getLength() == 0 && pTheirList->getLength() > 0)
	{
		return true;
	}

	int iOurValue = dealer.AI_dealVal(getID(), pOurList, false, iChange);
	const int iTheirValue = AI_dealVal(ePlayer, pTheirList, false, iChange);

	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			if (pTheirList->getLength() == 0)
			{
				return false;
			}
			//only accept 1 time lump sums, continuing gold per turn or resource per turn could be backstabbed
			for (CLLNode<TradeData>* pTheirNode = pTheirList->head(); pTheirNode; pTheirNode = pTheirList->next(pTheirNode))
			{
				if (pNode->m_data.m_eItemType == TRADE_GOLD_PER_TURN || pNode->m_data.m_eItemType == TRADE_DEFENSIVE_PACT || pNode->m_data.m_eItemType == TRADE_RESOURCES)
				{
					return false;
				}
			}
		}
	}

	if (iOurValue > 0 && 0 == pTheirList->getLength() && 0 == iTheirValue)
	{
		const CvTeamAI& dealerTeam = GET_TEAM(GET_PLAYER(ePlayer).getTeam());

		if (myTeam.isVassal(dealer.getTeam()) && CvDeal::isVassalTributeDeal(pOurList))
		{
			if (AI_getAttitude(ePlayer, false) > GC.getLeaderHeadInfo(getPersonalityType()).getVassalRefuseAttitudeThreshold()
			// OR I'm at war OR dealer has any defensive pact
			|| myTeam.isAtWar() || dealerTeam.getDefensivePactCount() != 0)
			{
				return true;
			}
			iOurValue *= 10 + myTeam.getPower(false);
			iOurValue /= 10 + dealerTeam.getPower(false);
		}
		else if (AI_getMemoryCount(ePlayer, MEMORY_MADE_DEMAND_RECENT) > 0
		|| AI_getAttitude(ePlayer) < ATTITUDE_PLEASED && myTeam.getPower(false) > dealerTeam.getPower(false) * 4 / 3)
		{
			return false;
		}

		int iThreshold = 2 * (myTeam.AI_getHasMetCounter(dealer.getTeam()) + 50);

		if (dealerTeam.AI_isLandTarget(getTeam()))
		{
			iThreshold *= 3;
		}

		iThreshold *= 100 + dealerTeam.getPower(false);
		iThreshold /= 100 + myTeam.getPower(false);

		iThreshold -= dealer.AI_getPeacetimeGrantValue(getID());

		return iOurValue < iThreshold;
	}

	if (iChange < 0)
	{
		return iTheirValue * 110 >= iOurValue * 100;
	}
	return iTheirValue >= iOurValue;
}


bool CvPlayerAI::AI_counterPropose(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirInventory, CLinkList<TradeData>* pOurInventory, CLinkList<TradeData>* pTheirCounter, CLinkList<TradeData>* pOurCounter) const
{
	PROFILE_EXTRA_FUNC();
	const bool bTheirGoldDeal = AI_goldDeal(pTheirList);
	const bool bOurGoldDeal = AI_goldDeal(pOurList);

	if (bOurGoldDeal && bTheirGoldDeal)
	{
		return false;
	}
	bool* pabBonusDeal = new bool[GC.getNumBonusInfos()];

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		pabBonusDeal[iI] = false;
	}
	CLLNode<TradeData>* pNode;
	CLLNode<TradeData>* pGoldPerTurnNode = NULL;
	CLLNode<TradeData>* pGoldNode = NULL;

	int iHumanDealWeight = AI_dealVal(ePlayer, pTheirList);
	int iAIDealWeight = GET_PLAYER(ePlayer).AI_dealVal(getID(), pOurList);

	pTheirCounter->clear();
	pOurCounter->clear();

	bool bOfferingCity = false;
	bool bReceivingCity = false;
	for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			bReceivingCity = true;
			break;
		}
	}
	for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
	{
		if (pNode->m_data.m_eItemType == TRADE_CITIES)
		{
			bOfferingCity = true;
			break;
		}
	}

	if (iAIDealWeight > iHumanDealWeight)
	{
		if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
		{
			int iBestValue = 0;
			int iBestWeight = 0;
			CLLNode<TradeData>* pBestNode = NULL;

			for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CITIES)
				{
					FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

					if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
					{
						CvCity* pCity = GET_PLAYER(ePlayer).getCity(pNode->m_data.m_iData);

						if (pCity != NULL)
						{
							const int iWeight = AI_cityTradeVal(pCity);

							if (iWeight > 0)
							{
								const int iValue = AI_targetCityValue(pCity, false);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestWeight = iWeight;
									pBestNode = pNode;
								}
							}
						}
					}
				}
			}
			if (pBestNode != NULL)
			{
				iHumanDealWeight += iBestWeight;
				pTheirCounter->insertAtEnd(pBestNode->m_data);
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_GOLD:
						if (!bOurGoldDeal)
						{
							pGoldNode = pNode;
						}
						break;
					case TRADE_GOLD_PER_TURN:
						if (!bOurGoldDeal)
						{
							pGoldPerTurnNode = pNode;
						}
						break;
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = GET_PLAYER(ePlayer).AI_maxGoldTrade(getID());

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					// If we can wrap this up with gold outright then do so.
					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iHumanDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pTheirCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		for (pNode = pOurList->head(); pNode; pNode = pOurList->next(pNode))
		{
			FAssert(!pNode->m_data.m_bHidden);

			switch (pNode->m_data.m_eItemType)
			{
			case TRADE_RESOURCES: pabBonusDeal[pNode->m_data.m_iData] = true;
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					int iWeight = 0;

					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_TECHNOLOGIES:
					{
						iWeight += GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), GET_PLAYER(ePlayer).getTeam());
						break;
					}
					case TRADE_RESOURCES:
					{
						if (!bOfferingCity && !pabBonusDeal[pNode->m_data.m_iData]
						&& GET_PLAYER(ePlayer).getNumTradeableBonuses((BonusTypes)pNode->m_data.m_iData) > 1
						&& GET_PLAYER(ePlayer).AI_corporationBonusVal((BonusTypes)(pNode->m_data.m_iData)) == 0)
						{
							iWeight += AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, 1);
							pabBonusDeal[pNode->m_data.m_iData] = true;
						}
						break;
					}
					}
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MAPS)
			{
				FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

				if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CONTACT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL && pNode->m_data.m_iData > -1)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)pNode->m_data.m_iData, GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0)
					{
						iHumanDealWeight += iWeight;
						pTheirCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}
		for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden
			&& (pNode->m_data.m_eItemType == TRADE_MILITARY_UNIT || pNode->m_data.m_eItemType == TRADE_WORKER))
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					CvUnit* pUnit = getUnit(pNode->m_data.m_iData);
					if (pUnit)
					{
						const int iWeight = std::max(GET_PLAYER(ePlayer).AI_militaryUnitTradeVal(pUnit), GET_PLAYER(ePlayer).AI_workerTradeVal(pUnit));
						if (iWeight > 0)
						{
							iHumanDealWeight += iWeight;
							pTheirCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = GET_PLAYER(ePlayer).AI_maxGoldTrade(getID());

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iHumanDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pTheirCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		if (!bOfferingCity && pGoldPerTurnNode)
		{
			const int iValueDiff = iAIDealWeight - iHumanDealWeight;
			if (iValueDiff > 0)
			{
				const int iTurns = getTreatyLength();
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				OutputDebugString(CvString::format("%S (%d)\n\tValueDiff=%d, iGoldValuePercent=%d, iGoldData=%d\n\t\tAI_getGoldValue=%d\n", getCivilizationDescription(0), getID(), iValueDiff, iGoldValuePercent, iGoldData, AI_getGoldValue(iGoldData, iGoldValuePercent)).c_str());

				// Account for rounding errors
				while (iGoldData < MAX_INT && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
				{
					iGoldData++;
					OutputDebugString(CvString::format("\tValueDiff=%d, iGoldValuePercent=%d, iGoldData=%d\n\t\tAI_getGoldValue=%d\n", iValueDiff, iGoldValuePercent, iGoldData, AI_getGoldValue(iGoldData, iGoldValuePercent)).c_str());
				}
				iGoldData = std::min(iGoldData / iTurns, GET_PLAYER(ePlayer).AI_maxGoldPerTurnTrade(getID()));

				if (iGoldData > 0)
				{
					const int iValue = AI_getGoldValue(iGoldData * iTurns, iGoldValuePercent);
					if (iValue > 0)
					{
						iHumanDealWeight += iValue;
						pGoldPerTurnNode->m_data.m_iData = iGoldData;
						pTheirCounter->insertAtEnd(pGoldPerTurnNode->m_data);
						pGoldPerTurnNode = NULL;
					}
				}
			}
		}

		if (!bOfferingCity)
		{
			for (pNode = pTheirInventory->head(); pNode && iAIDealWeight > iHumanDealWeight; pNode = pTheirInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_RESOURCES)
				{
					FAssert(GET_PLAYER(ePlayer).canTradeItem(getID(), pNode->m_data));

					if (GET_PLAYER(ePlayer).getTradeDenial(getID(), pNode->m_data) == NO_DENIAL && !pabBonusDeal[pNode->m_data.m_iData]
					&& GET_PLAYER(ePlayer).getNumTradeableBonuses((BonusTypes)pNode->m_data.m_iData) > 0)
					{
						pabBonusDeal[pNode->m_data.m_iData] = true;

						const int iWeight = AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), ePlayer, 1);

						if (iWeight > 0)
						{
							iHumanDealWeight += iWeight;
							pTheirCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}
	}
	else if (iHumanDealWeight > iAIDealWeight)
	{
		if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
		{
			bool bSurrender = false;
			for (pNode = pOurInventory->head(); pNode; pNode = pOurInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
				{
					if (pNode->m_data.m_eItemType == TRADE_SURRENDER)
					{
						if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
						{
							iAIDealWeight += GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_surrenderTradeVal(getTeam());
							pOurCounter->insertAtEnd(pNode->m_data);
							bSurrender = true;
						}
						break;
					}
				}
			}

			if (!bSurrender)
			{
				for (pNode = pOurInventory->head(); pNode; pNode = pOurInventory->next(pNode))
				{
					if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
					{
						if (pNode->m_data.m_eItemType == TRADE_PEACE_TREATY)
						{
							pOurCounter->insertAtEnd(pNode->m_data);
							break;
						}
					}
				}
			}

			int iBestValue = 0;
			int iBestWeight = 0;
			CLLNode<TradeData>* pBestNode = NULL;

			for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
			{
				if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CITIES)
				{
					FAssert(canTradeItem(ePlayer, pNode->m_data));

					if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
					{
						CvCity* pCity = getCity(pNode->m_data.m_iData);

						if (pCity != NULL)
						{
							const int iWeight = GET_PLAYER(ePlayer).AI_cityTradeVal(pCity);

							if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
							{
								const int iValue = GET_PLAYER(ePlayer).AI_targetCityValue(pCity, false);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestWeight = iWeight;
									pBestNode = pNode;
								}
							}
						}
					}
				}
			}
			if (pBestNode != NULL)
			{
				iAIDealWeight += iBestWeight;
				pOurCounter->insertAtEnd(pBestNode->m_data);
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					switch (pNode->m_data.m_eItemType)
					{
						case TRADE_GOLD:
						{
							if (!bTheirGoldDeal)
							{
								pGoldNode = pNode;
							}
							break;
						}
						case TRADE_GOLD_PER_TURN:
						{
							if (!bTheirGoldDeal)
							{
								pGoldPerTurnNode = pNode;
							}
							break;
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = AI_maxGoldTrade(ePlayer);

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					// If we can wrap this up with gold outright then do so.
					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iAIDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pOurCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}

		for (pNode = pTheirList->head(); pNode; pNode = pTheirList->next(pNode))
		{
			FAssert(!pNode->m_data.m_bHidden);

			switch (pNode->m_data.m_eItemType)
			{
			case TRADE_RESOURCES: pabBonusDeal[pNode->m_data.m_iData] = true;
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					int iWeight = 0;
					switch (pNode->m_data.m_eItemType)
					{
					case TRADE_TECHNOLOGIES:
					{
						iWeight += GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_techTradeVal((TechTypes)(pNode->m_data.m_iData), getTeam());
						break;
					}
					case TRADE_RESOURCES:
					{
						if (!pabBonusDeal[pNode->m_data.m_iData] && getNumTradeableBonuses((BonusTypes)(pNode->m_data.m_iData)) > 1)
						{
							iWeight += GET_PLAYER(ePlayer).AI_bonusTradeVal(((BonusTypes)(pNode->m_data.m_iData)), getID(), 1);
							pabBonusDeal[pNode->m_data.m_iData] = true;
						}
						break;
					}
					}
					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MAPS)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					const int iWeight = GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_mapTradeVal(getTeam());

					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}

		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_CONTACT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL && pNode->m_data.m_iData > -1)
				{
					const int iWeight = GET_TEAM(getTeam()).AI_contactTradeVal((TeamTypes)pNode->m_data.m_iData, GET_PLAYER(ePlayer).getTeam());
					if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
					{
						iAIDealWeight += iWeight;
						pOurCounter->insertAtEnd(pNode->m_data);
					}
				}
			}
		}
		for (pNode = pOurInventory->head(); pNode && iHumanDealWeight > iAIDealWeight; pNode = pOurInventory->next(pNode))
		{
			if (!pNode->m_data.m_bOffering && !pNode->m_data.m_bHidden && pNode->m_data.m_eItemType == TRADE_MILITARY_UNIT)
			{
				FAssert(canTradeItem(ePlayer, pNode->m_data));

				if (getTradeDenial(ePlayer, pNode->m_data) == NO_DENIAL)
				{
					CvUnit* pUnit = getUnit(pNode->m_data.m_iData);
					if (pUnit)
					{
						const int iWeight = AI_militaryUnitTradeVal(pUnit);
						if (iWeight > 0 && iHumanDealWeight >= iAIDealWeight + iWeight)
						{
							iAIDealWeight += iWeight;
							pOurCounter->insertAtEnd(pNode->m_data);
						}
					}
				}
			}
		}

		if (pGoldNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
				if (iGoldData > 0)
				{
					const int iMaxTrade = AI_maxGoldTrade(ePlayer);

					if (iGoldData > iMaxTrade)
					{
						iGoldData = iMaxTrade;
					}
					else
					{
						// Account for rounding errors
						while (iGoldData < iMaxTrade && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
						{
							iGoldData++;
						}
					}

					if (iGoldData > 0)
					{
						const int iValue = AI_getGoldValue(iGoldData, iGoldValuePercent);
						if (iValue > 0)
						{
							iAIDealWeight += iValue;
							pGoldNode->m_data.m_iData = iGoldData;
							pOurCounter->insertAtEnd(pGoldNode->m_data);
							pGoldNode = NULL;
						}
					}
				}
			}
		}
		if (pGoldPerTurnNode)
		{
			const int iValueDiff = iHumanDealWeight - iAIDealWeight;
			if (iValueDiff > 0)
			{
				const int iTurns = getTreatyLength();
				const int iGoldValuePercent = AI_goldTradeValuePercent();
				int iGoldData = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);

				// Account for rounding errors
				while (iGoldData < MAX_INT && AI_getGoldValue(iGoldData, iGoldValuePercent) < iValueDiff)
				{
					iGoldData++;
				}
				iGoldData = std::min(iGoldData / iTurns, AI_maxGoldPerTurnTrade(ePlayer));

				if (iGoldData > 0)
				{
					const int iValue = AI_getGoldValue(iGoldData * iTurns, iGoldValuePercent);
					if (iValue > 0)
					{
						iAIDealWeight += iValue;
						pGoldPerTurnNode->m_data.m_iData = iGoldData;
						pOurCounter->insertAtEnd(pGoldPerTurnNode->m_data);
						pGoldPerTurnNode = NULL;
					}
				}
			}
		}
	}
	SAFE_DELETE_ARRAY(pabBonusDeal);

	return ((iAIDealWeight <= iHumanDealWeight) && ((pOurList->getLength() > 0) || (pOurCounter->getLength() > 0) || (pTheirCounter->getLength() > 0)));
}


int CvPlayerAI::AI_maxGoldTrade(PlayerTypes ePlayer) const
{
	int64_t iMaxGold;

	FAssert(ePlayer != getID());

	if (isHumanPlayer() || GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		const int64_t iMaxGold = getGold();
		return iMaxGold < MAX_INT ? static_cast<int>(iMaxGold) : MAX_INT;
	}
	const int64_t iGold = getGold();
	iMaxGold = iGold;

	iMaxGold *= GC.getLeaderHeadInfo(getPersonalityType()).getMaxGoldTradePercent();
	iMaxGold /= 100;

	const int iGoldRate = calculateGoldRate();

	if (iGoldRate < 0)
	{
		iMaxGold = std::min<int64_t>(iMaxGold, iGold + iGoldRate * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / 10);
	}
	if (iMaxGold > 0)
	{
		iMaxGold = std::min<int64_t>(iMaxGold, iGold);

		iMaxGold -= (iMaxGold % GC.getDIPLOMACY_VALUE_REMAINDER());

		return iMaxGold < MAX_INT ? static_cast<int>(iMaxGold) : MAX_INT;
	}
	return 0;
}


int CvPlayerAI::AI_maxGoldPerTurnTrade(PlayerTypes ePlayer) const
{
	int64_t iMaxGoldPerTurn;

	FAssert(ePlayer != getID());

	if (isHumanPlayer() || (GET_PLAYER(ePlayer).getTeam() == getTeam()))
	{
		iMaxGoldPerTurn = calculateGoldRate() + getGold() / getTreatyLength();
	}
	else
	{
		iMaxGoldPerTurn = getTotalPopulation();

		iMaxGoldPerTurn *= GC.getLeaderHeadInfo(getPersonalityType()).getMaxGoldPerTurnTradePercent();
		iMaxGoldPerTurn /= 100;

		iMaxGoldPerTurn += std::min(0, getGoldPerTurnByPlayer(ePlayer));
	}

	return std::max(0, (int)std::min<int64_t>(iMaxGoldPerTurn, calculateGoldRate()));
}


// Toffer - Gold 2 Value & Value 2 Gold
int CvPlayerAI::AI_getGoldValue(const int iGold, const int iValuePercent) const
{
	return static_cast<int>(std::min<uint64_t>(iGold * iValuePercent / GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent(), MAX_INT));
}
int CvPlayerAI::AI_getGoldFromValue(const int iValue, const int iValuePercent) const
{
	return static_cast<int>(std::min<uint64_t>(iValue * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / iValuePercent, MAX_INT));
}
// ! Toffer


int CvPlayerAI::AI_bonusVal(BonusTypes eBonus, int iChange, bool bForTrade) const
{
	PROFILE_FUNC();

	if (iChange < 2 && iChange > -2)
	{
		if (iChange == 0)
		{
			return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade);
		}
		const int iBonusCount = getNumAvailableBonuses(eBonus);
		if (iChange == 1 && iBonusCount == 0 || iChange == -1 && iBonusCount == 1)
		{
			//This is assuming the none-to-one or one-to-none case.
			return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade);
		}
	}
	//This is basically the marginal value of an additional instance of a bonus.
	return AI_corporationBonusVal(eBonus) + AI_baseBonusVal(eBonus, bForTrade) / 5;
}

//Value sans corporation
int CvPlayerAI::AI_baseBonusVal(BonusTypes eBonus, bool bForTrade) const
{
	PROFILE_FUNC();

	//recalculate if needed
	if (m_aiBonusValue[eBonus] == -1 || !bForTrade && !m_abNonTradeBonusCalculated[eBonus])
	{
		PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate");

		int iValue = 0;
		int iTradeValue = 0;
		//	If we've already calculated everythign except the not-currently-constructable
		//	buildings (which only appky to the non-trade value) and now we need the full
		//	non-trade value thn we just need to add the contriobution from those buildings
		const bool bJustNonTradeBuildings = (m_aiBonusValue[eBonus] != -1);
		CvTeam& kTeam = GET_TEAM(getTeam());

		if (!kTeam.isBonusObsolete(eBonus))
		{
			CvCity* pCapital = getCapitalCity();
			int iCoastalCityCount = countNumCoastalCities();

			// find the first coastal city
			CvCity* pCoastalCity = NULL;
			if (iCoastalCityCount > 0)
			{
				pCoastalCity = findBestCoastalCity();
			}
			const int iCityCount = getNumCities();
			const int iCityCountNonZero = std::max(1, iCityCount);

			if (!bJustNonTradeBuildings)
			{
				iValue += (GC.getBonusInfo(eBonus).getHappiness() * 100);
				iValue += (GC.getBonusInfo(eBonus).getHealth() * 100);

				iTradeValue = iValue;

				{
					PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Unit Value");
					for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
					{
						const CvUnitInfo& kLoopUnit = GC.getUnitInfo((UnitTypes)iI);

						//	Don't consider units more than one era ahead of us
						if (kLoopUnit.getPrereqAndTech() != NO_TECH)
						{
							const CvTechInfo& prereqTech = GC.getTechInfo((TechTypes)kLoopUnit.getPrereqAndTech());

							if (prereqTech.getEra() > (int)getCurrentEra() + 1)
							{
								continue;
							}
						}

						int iBonusORVal = 0;
						int	iHasOther = 0;

						foreach_(const BonusTypes ePrereqBonus, kLoopUnit.getPrereqOrBonuses())
						{
							if (ePrereqBonus == eBonus)
							{
								iBonusORVal = 40;
							}
							else if (getNumAvailableBonuses(ePrereqBonus) > 0)
							{
								iHasOther += getNumAvailableBonuses(ePrereqBonus);
							}
						}

						while (iHasOther-- > 0)
						{
							iBonusORVal /= 4;
						}

						int iTempValue = iBonusORVal + kLoopUnit.getBonusProductionModifier(eBonus) / 10;

						if (kLoopUnit.getPrereqAndBonus() == eBonus)
						{
							iTempValue += 50;
						}

						if (iTempValue > 0)
						{
							const bool bIsWater = kLoopUnit.getDomainType() == DOMAIN_SEA;

							// if non-limited water unit, weight by coastal cities
							if (bIsWater && !isLimitedUnit((UnitTypes)iI))
							{
								iTempValue *= std::min(iCoastalCityCount * 2, iCityCount);	// double coastal cities, cap at total cities
								iTempValue /= iCityCountNonZero;
							}
							int iTempTradeValue = 0;

							// is it a water unit and no coastal cities
							if (bIsWater && pCoastalCity == NULL)
							{
								//	worthless
								iTempValue = 2;

								iTempTradeValue = iTempValue;
							}
							else if (canTrain((UnitTypes)iI))
							{
								// is it a water unit and no coastal cities or our coastal city cannot build because its obsolete
								if ((bIsWater && (pCoastalCity->allUpgradesAvailable((UnitTypes)iI) != NO_UNIT)) ||
									// or our capital cannot build because its obsolete (we can already build all its upgrades)
									(pCapital != NULL && pCapital->allUpgradesAvailable((UnitTypes)iI) != NO_UNIT))
								{
									// its worthless
									iTempValue = 2;
								}
								// otherwise, value units we could build if we had this bonus double
								else
								{
									iTempValue *= 2;
								}
								iTempTradeValue = iTempValue;
							}
							else // Trades are short-term - if we can't train the unit now assume we won't be able to do so for the duration of the trade
							{
								iTempTradeValue = 0;
							}

							if (iTempValue > 0)
							{
								int	iTechDistance = 0;

								// If building this is dependent (directly or otherwise) on a tech, assess how
								//	distant that tech is
								if (kLoopUnit.getPrereqAndTech() != NO_TECH)
								{
									iTechDistance = std::max(iTechDistance, findPathLength((TechTypes)kLoopUnit.getPrereqAndTech(), false));
								}
								//	Without some more checks we are over-assessing religious buildings a lot
								//	so if there is a religion pre-req make some basic checks on the availability of
								//	the religion
								if (kLoopUnit.getPrereqReligion() != NO_RELIGION)
								{
									const CvReligionInfo& kReligion = GC.getReligionInfo((ReligionTypes)kLoopUnit.getPrereqReligion());

									iTechDistance = std::max(iTechDistance, findPathLength(kReligion.getTechPrereq(), false));
								}
								//	Similarly corporations
								if (kLoopUnit.getPrereqCorporation() != NO_RELIGION)
								{
									const CvCorporationInfo& kCorporation = GC.getCorporationInfo((CorporationTypes)kLoopUnit.getPrereqCorporation());

									iTechDistance = std::max(iTechDistance, findPathLength(kCorporation.getTechPrereq(), false));
								}

								iTempValue = (iTempValue * 15) / (10 + iTechDistance);
								iTempTradeValue = (iTempTradeValue * 15) / (10 + iTechDistance);
							}

							iValue += iTempValue;
							iTradeValue += iTempTradeValue;
						}
					}
				}
			}

			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Building Value");
				for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
				{
					const BuildingTypes eBuildingX = static_cast<BuildingTypes>(iI);

					if (!GET_TEAM(getTeam()).isObsoleteBuilding(eBuildingX))
					{
						const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eBuildingX);
						bool bCanConstruct = false;

						if (bJustNonTradeBuildings || bForTrade)
						{
							bCanConstruct = canConstruct(eBuildingX, false, /*bTestVisible*/ true, /*bIgnoreCost*/ true);

							if (bCanConstruct == bJustNonTradeBuildings)
							{
								continue;
							}
						}
						const TechTypes eBuildingTech = (TechTypes)kLoopBuilding.getPrereqAndTech();
						int iBuildingTechDistance = MAX_INT;

						if (eBuildingTech == NO_TECH)
						{
							iBuildingTechDistance = 0;
						}
						else if (GC.getTechInfo(eBuildingTech).getEra() <= getCurrentEra() + 1)
						{
							iBuildingTechDistance = findPathLength((TechTypes)kLoopBuilding.getPrereqAndTech(), false);
						}

						if (iBuildingTechDistance < 15)
						{
							if (!bJustNonTradeBuildings && !bForTrade)
							{
								bCanConstruct = canConstruct(eBuildingX, false, /*bTestVisible*/ true, /*bIgnoreCost*/ true);
							}
							bool bCouldConstruct = false;
							bool bCanConstructAnyway = true;

							if (kLoopBuilding.getPrereqAndBonus() != NO_BONUS)
							{
								if (kLoopBuilding.getPrereqAndBonus() == eBonus)
								{
									bCouldConstruct = true;
									bCanConstructAnyway = false;
								}
								else if (!hasBonus((BonusTypes)kLoopBuilding.getPrereqAndBonus()))
								{
									bCanConstructAnyway = false;
								}
							}

							if (bCouldConstruct || bCanConstructAnyway)
							{
								bool bHasOR = false;
								bool bGetsOR = false;
								bool bRequiresOR = false;

								foreach_(const BonusTypes ePrereqBonus, kLoopBuilding.getPrereqOrBonuses())
								{
									bRequiresOR = true;

									if (ePrereqBonus == eBonus)
									{
										bGetsOR = true;
									}
									else if (hasBonus(ePrereqBonus))
									{
										bHasOR = true;
										break;
									}
								}
								if (bRequiresOR)
								{
									if (!bHasOR)
									{
										bCanConstructAnyway = false;
									}
									else if (bGetsOR)
									{
										bCouldConstruct |= bCanConstructAnyway;
									}
								}
							}

							if (bCouldConstruct || bCanConstructAnyway)
							{
								int iTempValue = kLoopBuilding.getBonusProductionModifier(eBonus) / 10;

								if (kLoopBuilding.getPowerBonus() == eBonus)
								{
									iTempValue += 60;
								}
								const int iBuildingCount = std::max(1, getBuildingCount(eBuildingX));

								for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
								{
									iTempValue += kLoopBuilding.getBonusYieldModifier(eBonus, iJ) / 2;
									if (kLoopBuilding.getPowerBonus() == eBonus)
									{
										iTempValue += kLoopBuilding.getPowerYieldModifier(iJ);
									}
									// Remember, these are all divided by 10 at the end...
									// Divide by number of cities as this is supposed to be a per-city value.
									iTempValue += (kLoopBuilding.getBonusYieldChanges(eBonus, iJ) * iBuildingCount * 60) / std::max(1, getNumCities());
									iTempValue += (kLoopBuilding.getBonusYieldModifier(eBonus, iJ) * iBuildingCount * 10) / std::max(1, getNumCities());
								}

								for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
								{
									//	Percent modifier on commerce estimate per city as that modifer applied to the player's total commerce divided by num cities
									iTempValue += kLoopBuilding.getBonusCommercePercentChanges(eBonus, iJ) * iBuildingCount * getCommerceRate((CommerceTypes)iJ) / (10 * std::max(1, getNumCities() * getNumCities()));
									iTempValue += kLoopBuilding.getBonusCommerceModifier(eBonus, iJ) * iBuildingCount * 10 / std::max(1, getNumCities());
								}
								iTempValue += iBuildingCount * kLoopBuilding.getBonusHappinessChanges().getValue(eBonus) * 120 / std::max(1, getNumCities());
								iTempValue += iBuildingCount * kLoopBuilding.getBonusHealthChanges().getValue(eBonus) * 80 / std::max(1, getNumCities());
								iTempValue += iBuildingCount * kLoopBuilding.getBonusDefenseChanges(eBonus) * 10 / std::max(1, getNumCities());

								int iTempNonTradeValue = 0;
								int iTempTradeValue = 0;
								// determine whether we have the tech for this building
								{
									bool bHasTechForBuilding = kTeam.isHasTech((TechTypes)kLoopBuilding.getPrereqAndTech());
									if (bHasTechForBuilding)
									{
										foreach_(const TechTypes ePrereqTech, kLoopBuilding.getPrereqAndTechs())
										{
											if (!kTeam.isHasTech(ePrereqTech))
											{
												bHasTechForBuilding = false;
											}
										}
									}
									// It only has worth if we can potentially build it.
									// When statement is False it is accurate, it may be true in some cases where we will never be able to build
									if (!bHasTechForBuilding || bCanConstruct || kLoopBuilding.getPrereqStateReligion() > -1)
									{
										if (bCouldConstruct && !bCanConstructAnyway)
										{
											iTempNonTradeValue += 100;
										}
										if (bCanConstruct)
										{
											// double value if we can build it right now
											iTempNonTradeValue += iTempValue;
											iTempNonTradeValue *= 2;
											iTempTradeValue += 2 * iTempValue;
										}
									}
								}
								iTempValue = 0;

								// if non-limited water building, weight by coastal cities
								if (kLoopBuilding.isWater() && !isLimitedWonder(eBuildingX))
								{
									iTempNonTradeValue *= iCoastalCityCount;
									iTempNonTradeValue /= std::max(1, iCityCount / 2);

									iTempTradeValue *= iCoastalCityCount;
									iTempTradeValue /= std::max(1, iCityCount / 2);
								}

								if (iTempNonTradeValue > 0 || iTempTradeValue > 0)
								{
									int	iTechDistance = iBuildingTechDistance;

									//	Without some more checks we are over-assessing religious buildings a lot
									//	so if there is a religion pre-req make some basic checks on the availability of
									//	the religion
									const ReligionTypes eReligion = (ReligionTypes)kLoopBuilding.getPrereqReligion();
									if (eReligion != NO_RELIGION)
									{
										// Trade is short term - don't assume useful religion spread,
										//	just weight by the cities that have the religion already
										iTempTradeValue = iTempTradeValue * getHasReligionCount(eReligion) / iCityCountNonZero;

										iTechDistance = std::max(iTechDistance, findPathLength(GC.getReligionInfo(eReligion).getTechPrereq(), false));
									}

									// Similarly corporations
									const CorporationTypes eCorporation = (CorporationTypes)kLoopBuilding.getPrereqCorporation();
									if (eCorporation != NO_CORPORATION)
									{
										// Trade is short term - don't assume useful corporation spread - just weight by the cities that have it already
										iTempTradeValue = iTempTradeValue * getHasCorporationCount(eCorporation) / iCityCountNonZero;

										const TechTypes eCorpTech = GC.getCorporationInfo(eCorporation).getTechPrereq();
										if (eCorpTech > NO_TECH)
										{
											iTechDistance = std::max(iTechDistance, findPathLength(eCorpTech, false));
										}
									}

									if (iTempNonTradeValue > 0 && !bCanConstruct)
									{
										// If building this is dependent (directly or otherwise) on a tech, assess how
										//	distant that tech is
										iTempNonTradeValue = (iTempNonTradeValue * 15) / (10 + iTechDistance);
									}
									iTempTradeValue = (iTempTradeValue * 15) / (10 + iTechDistance);
								}
								//Special Wonder Considerations...
								if (isLimitedWonder(eBuildingX) && bCanConstruct)
								{
									// World wonders we are competing for so boost them higher
									const int iWonderModifier = (isWorldWonder(eBuildingX) ? 3 : 1) * kLoopBuilding.getBonusProductionModifier(eBonus);
									iTempTradeValue = iTempTradeValue * iWonderModifier / 100;
									iTempNonTradeValue = iTempNonTradeValue * iWonderModifier / 100;
								}

								// Trades are short-term - if we can't construct the building now assume we won't be able to do so for the duration of the trade
								if (!bCanConstruct)
								{
									iTempTradeValue = 0;
								}
								// Buildings will beconme diabled once their pre-reqs are no longer present so give them less weight for temporary trades
								iValue += iTempNonTradeValue;
								iTradeValue += iTempTradeValue / 3;
							}
						}
					}
				}
			}

			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Project Value");

				for (int iI = 0; iI < GC.getNumProjectInfos(); iI++)
				{
					const ProjectTypes eProject = static_cast<ProjectTypes>(iI);
					const CvProjectInfo& kLoopProject = GC.getProjectInfo(eProject);

					int iTempValue = kLoopProject.getBonusProductionModifier(eBonus) / 10;

					if (iTempValue > 0)
					{
						int iTempTradeValue = 0;
						int iTempNonTradeValue = 0;

						if (!GC.getGame().isProjectMaxedOut(eProject) && !kTeam.isProjectMaxedOut(eProject))
						{
							if (canCreate(eProject))
							{
								iTempValue *= 2;
								iTempTradeValue += iTempValue;
							}
							// Trades are short-term - if we can't construct the building now assume we won't be able to do so for the duration of the trade
							iTempNonTradeValue += iTempValue;
						}

						if (kLoopProject.getTechPrereq() != NO_TECH)
						{
							const int iDiff = abs(GC.getTechInfo(kLoopProject.getTechPrereq()).getEra() - getCurrentEra());

							if (iDiff == 0)
							{
								iTempTradeValue *= 3;
								iTempTradeValue /= 2;

								iTempNonTradeValue *= 3;
								iTempNonTradeValue /= 2;
							}
							else
							{
								iTempTradeValue /= iDiff;
								iTempNonTradeValue /= iDiff;
							}
						}
						iValue += iTempNonTradeValue;
						iTradeValue += iTempTradeValue;
					}
				}
			}


			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Route Value");
				RouteTypes eBestRoute = getBestRoute();
				for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
				{
					RouteTypes eRoute = (RouteTypes)(GC.getBuildInfo((BuildTypes)iI).getRoute());

					if (eRoute != NO_ROUTE)
					{
						int iTempValue = 0;
						if (GC.getRouteInfo(eRoute).getPrereqBonus() == eBonus)
						{
							iTempValue += 80;
						}
						if (algo::any_of_equal(GC.getRouteInfo(eRoute).getPrereqOrBonuses(), eBonus))
						{
							iTempValue += 40;
						}
						if (eBestRoute != NO_ROUTE && GC.getRouteInfo(getBestRoute()).getValue() > GC.getRouteInfo(eRoute).getValue())
						{
							iTempValue /= 2;
						}
						iValue += iTempValue;
						iTradeValue += iTempValue;
					}
				}
			}

			//Resource scarcity. If there are only limited quantities of this resource, treasure it.
			if (!bJustNonTradeBuildings)
			{
				PROFILE("CvPlayerAI::AI_baseBonusVal::recalculate Bonus Scarcity");
				int iTotalBonusCount = 0;
				for (int iI = 0; iI < MAX_PLAYERS; iI++)
				{
					if (GET_PLAYER((PlayerTypes)iI).isAlive()
					&& GET_TEAM(getTeam()).isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						iTotalBonusCount += GET_PLAYER((PlayerTypes)iI).getNumAvailableBonuses(eBonus);
					}
				}
				const int iTempValue =
					(
						GC.getBonusInfo(eBonus).getAIObjective() * 10
						+
						getNumAvailableBonuses(eBonus) * 300 / std::max(1, iTotalBonusCount)
					);
				iValue += iTempValue;
				iTradeValue += iTempValue;
			}
			iValue /= 10;
			// All these effects are only going to be with us for a short period so devalue
			iTradeValue /= 30;
		}

		//	Check there wasn't a race copndition that meant some other thread already did this
		if (m_aiBonusValue[eBonus] == -1 || !bForTrade && !m_abNonTradeBonusCalculated[eBonus])
		{
			if (!bJustNonTradeBuildings)
			{
				m_aiBonusValue[eBonus] = std::max(0, iValue);
				m_aiTradeBonusValue[eBonus] = std::max(0, iTradeValue);
			}
			else
			{
				m_aiBonusValue[eBonus] += std::max(0, iValue);
			}
			m_abNonTradeBonusCalculated[eBonus] |= !bForTrade;
		}
	}
	return (bForTrade ? m_aiTradeBonusValue[eBonus] : m_aiBonusValue[eBonus]);
}

int CvPlayerAI::AI_corporationBonusVal(BonusTypes eBonus) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	int iCityCount = getNumCities();
	iCityCount += iCityCount / 6 + 1;

	for (int iCorporation = 0; iCorporation < GC.getNumCorporationInfos(); ++iCorporation)
	{
		int iCorpCount = getHasCorporationCount((CorporationTypes)iCorporation);
		if (iCorpCount > 0)
		{
			iCorpCount += getNumCities() / 6 + 1;
			const CvCorporationInfo& kCorp = GC.getCorporationInfo((CorporationTypes)iCorporation);
			foreach_(const BonusTypes ePrereqBonus, kCorp.getPrereqBonuses())
			{
				if (eBonus == ePrereqBonus)
				{
					iValue += (50 * kCorp.getYieldProduced(YIELD_FOOD) * iCorpCount) / iCityCount;
					iValue += (50 * kCorp.getYieldProduced(YIELD_PRODUCTION) * iCorpCount) / iCityCount;
					iValue += (30 * kCorp.getYieldProduced(YIELD_COMMERCE) * iCorpCount) / iCityCount;

					iValue += (30 * kCorp.getCommerceProduced(COMMERCE_GOLD) * iCorpCount) / iCityCount;
					iValue += (30 * kCorp.getCommerceProduced(COMMERCE_RESEARCH) * iCorpCount) / iCityCount;
					iValue += (12 * kCorp.getCommerceProduced(COMMERCE_CULTURE) * iCorpCount) / iCityCount;
					iValue += (20 * kCorp.getCommerceProduced(COMMERCE_ESPIONAGE) * iCorpCount) / iCityCount;

					//Disabled since you can't found/spread a corp unless there is already a bonus,
					//and that bonus will provide the entirity of the bonusProduced benefit.

					/*if (NO_BONUS != kCorp.getBonusProduced())
					{
						if (getNumAvailableBonuses((BonusTypes)kCorp.getBonusProduced()) == 0)
						{
							iBonusValue += (1000 * iCorpCount * AI_baseBonusVal((BonusTypes)kCorp.getBonusProduced())) / (10 * iCityCount);
					}
					}*/
				}
			}
		}
	}

	iValue /= 100;	//percent
	iValue /= 10;	//match AI_baseBonusVal

	return iValue;
}


int CvPlayerAI::AI_bonusTradeVal(BonusTypes eBonus, PlayerTypes ePlayer, int iChange) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	int iValue = AI_bonusVal(eBonus, iChange, true);

	iValue *= 30 * (std::min(getNumCities(), GET_PLAYER(ePlayer).getNumCities()) + 3);
	iValue /= 100;

	iValue = getModifiedIntValue(iValue, GC.getBonusInfo(eBonus).getAITradeModifier());

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()) && !GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isCapitulated())
	{
		iValue /= 2;
	}
	return iValue * getTreatyLength();
}


DenialTypes CvPlayerAI::AI_bonusTrade(BonusTypes eBonus, PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");

	if (isHumanPlayer() && GET_PLAYER(ePlayer).isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getNumAvailableBonuses(eBonus) > 0 && GET_PLAYER(ePlayer).AI_corporationBonusVal(eBonus) <= 0)
	{
		return (GET_PLAYER(ePlayer).isHumanPlayer() ? DENIAL_JOKING : DENIAL_NO_GAIN);
	}

	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).AI_getWorstEnemy() == GET_PLAYER(ePlayer).getTeam())
	{
		return DENIAL_WORST_ENEMY;
	}

	if (AI_corporationBonusVal(eBonus) > 0)
	{
		return DENIAL_JOKING;
	}

	bool bStrategic = false;

	// Disregard obsolete units
	const CvCity* pCapitalCity = getCapitalCity();
	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (!GC.getGame().canEverTrain((UnitTypes)iI)
		|| pCapitalCity->allUpgradesAvailable((UnitTypes)iI) != NO_UNIT)
		{
			continue;
		}

		if (GC.getUnitInfo((UnitTypes)iI).getPrereqAndBonus() == eBonus
		|| algo::any_of_equal(GC.getUnitInfo((UnitTypes)iI).getPrereqOrBonuses(), eBonus))
		{
			bStrategic = true;
			break;
		}
	}

	if (!bStrategic)
	{
		for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
		{
			if (GET_TEAM(getTeam()).isObsoleteBuilding((BuildingTypes)iI)
			|| !GC.getGame().canEverConstruct((BuildingTypes)iI))
			{
				continue;
			}

			if (GC.getBuildingInfo((BuildingTypes)iI).getPrereqAndBonus() == eBonus
			|| algo::any_of_equal(GC.getBuildingInfo((BuildingTypes)iI).getPrereqOrBonuses(), eBonus))
			{
				bStrategic = true;
				break;
			}
		}
	}
	// XXX marble and stone???

	const AttitudeTypes eAttitude = AI_getAttitude(ePlayer);

	if (bStrategic)
	{
		//If we are planning war, don't sell our resources!
		if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS) && GET_TEAM(getTeam()).hasWarPlan(true))
		{
			return DENIAL_MYSTERY;
		}

		if (eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getStrategicBonusRefuseAttitudeThreshold())
		{
			return DENIAL_ATTITUDE;
		}
	}

	if (GC.getBonusInfo(eBonus).getHappiness() > 0
	&& eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getHappinessBonusRefuseAttitudeThreshold())
	{
		return DENIAL_ATTITUDE;
	}

	if (GC.getBonusInfo(eBonus).getHealth() > 0
	&& eAttitude <= GC.getLeaderHeadInfo(getPersonalityType()).getHealthBonusRefuseAttitudeThreshold())
	{
		return DENIAL_ATTITUDE;
	}

	return NO_DENIAL;
}


int CvPlayerAI::AI_cityTradeVal(CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pCity->getOwner() != getID());

	int iValue = 500;
	//consider infrastructure
	{
		foreach_(const BuildingTypes eType, pCity->getHasBuildings())
		{
			if (isWorldWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 3;
			}
			else if (isLimitedWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 5;
			}
			else
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 10;
			}
		}
	}
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHasReligion((ReligionTypes)iI) && getStateReligion() == iI)
		{
			iValue += 100;
			if (pCity->isHolyCity((ReligionTypes)iI))
			{
				iValue += 500;
			}
			break;
		}
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (pCity->isHasCorporation((CorporationTypes)iI))
		{
			iValue += AI_corporationValue((CorporationTypes)iI, pCity) / 25;
			if (pCity->isHeadquarters((CorporationTypes)iI))
			{
				iValue += AI_corporationTradeVal((CorporationTypes)iI);
			}
		}
	}

	iValue += (pCity->getPopulation() * 50);

	iValue += (pCity->getCultureLevel() * 200);

	iValue += (((GC.getGame().getElapsedGameTurns() + 100) * 4) * pCity->plot()->calculateCulturePercent(pCity->getOwner())) / 400;

	foreach_(const CvPlot * pLoopPlot, pCity->plots())
	{
		if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
		{
			iValue += (AI_bonusVal(pLoopPlot->getBonusType(getTeam())) * 10);
		}
	}

	//	Add in a multiple of what it produces each turn
	int iCommercePerTurn = pCity->getCommerceRate(COMMERCE_GOLD) + pCity->getCommerceRate(COMMERCE_RESEARCH) + pCity->getCommerceRate(COMMERCE_CULTURE) + pCity->getCommerceRate(COMMERCE_ESPIONAGE);
	iValue += 6 * iCommercePerTurn;

	//	Don't count food - it doesn't contribute globally to the civ so population is a proxy
	int iYieldPerTurn = pCity->getYieldRate(YIELD_PRODUCTION);
	iValue += 12 * iYieldPerTurn;

	if (!(pCity->isEverOwned(getID())))
	{
		iValue *= 3;
		iValue /= 2;
	}
	//in danger
	if (pCity->AI_isDanger() && !pCity->AI_isDefended(strengthOfBestUnitAI(DOMAIN_LAND, UNITAI_CITY_DEFENSE)))
	{
		iValue *= 2;
		iValue /= 3;
	}
	//unstable
	if (pCity->getRevolutionIndex() > 1000)
	{
		iValue *= 2;
		iValue /= 3;
	}
	//colony
	if (!GC.getMap().getArea(pCity->getArea())->isHomeArea(GET_PLAYER(pCity->getOwner()).getID()))
	{
		iValue *= 4;
		iValue /= 5;
	}
	//This city costs money, and we can't afford it
	if (AI_isFinancialTrouble() && (pCity->getCommerceRateTimes100(COMMERCE_GOLD) - pCity->getMaintenanceTimes100() < 0))
	{
		iValue /= 2;
	}
	return iValue * 15;
}


int CvPlayerAI::AI_ourCityValue(CvCity* pCity) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 150;
	//consider infrastructure
	{
		foreach_(const BuildingTypes eType, pCity->getHasBuildings())
		{
			if (isWorldWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 3;
			}
			else if (isLimitedWonder(eType))
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 5;
			}
			else
			{
				iValue += GC.getBuildingInfo(eType).getProductionCost() / 10;
			}
		}
	}
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (pCity->isHasReligion((ReligionTypes)iI) && getStateReligion() == iI)
		{
			iValue += 100;
			if (pCity->isHolyCity((ReligionTypes)iI))
			{
				iValue += 500;
			}
			break;
		}
	}
	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (pCity->isHasCorporation((CorporationTypes)iI))
		{
			iValue += AI_corporationValue((CorporationTypes)iI, pCity) / 25;
			if (pCity->isHeadquarters((CorporationTypes)iI))
			{
				iValue += AI_corporationTradeVal((CorporationTypes)iI);
			}
		}
	}
	iValue += (pCity->getPopulation() * 50);

	iValue += (pCity->getCultureLevel() * 200);

	iValue += (((GC.getGame().getElapsedGameTurns() + 100) * 4) * pCity->plot()->calculateCulturePercent(pCity->getOwner())) / 400;

	foreach_(const CvPlot * pLoopPlot, pCity->plots())
	{
		if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
		{
			iValue += (AI_bonusVal(pLoopPlot->getBonusType(getTeam())) * 10);
		}
	}

	//in danger
	if (pCity->AI_isDanger() && !pCity->AI_isDefended(strengthOfBestUnitAI(DOMAIN_LAND, UNITAI_CITY_DEFENSE))) {
		iValue *= 2;
		iValue /= 3;
	}
	//unstable
	if (pCity->getRevolutionIndex() > 1000) {
		iValue *= 2;
		iValue /= 3;
	}
	//colony
	if (!GC.getMap().getArea(pCity->getArea())->isHomeArea(getID())) {
		iValue *= 4;
		iValue /= 5;
	}
	//This city is costing us money, and we can't afford it
	if (AI_isFinancialTrouble() && (pCity->getCommerceRateTimes100(COMMERCE_GOLD) - pCity->getMaintenanceTimes100() < 0)) {
		iValue /= 2;
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_cityTrade(CvCity* pCity, PlayerTypes ePlayer) const
{
	FAssert(pCity->getOwner() == getID()); // I can only sell cities that I own.

	if (pCity->getLiberationPlayer(false) == ePlayer)
	{
		return NO_DENIAL;
	}

	if (pCity->getID() == getCapitalCity()->getID())
	{
		return DENIAL_JOKING; // Toffer - Hmm, maybe this should be before the liberation "No Denial"?
	}

	if (isHumanPlayer() || atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).AI_isFinancialTrouble())
	{
		return DENIAL_MYSTERY;
	}
	{
		const CvCity* pNearestCity = GC.getMap().findCity(pCity->getX(), pCity->getY(), ePlayer, NO_TEAM, true, false, NO_TEAM, NO_DIRECTION, pCity);

		if (pNearestCity == NULL || plotDistance(pCity->getX(), pCity->getY(), pNearestCity->getX(), pNearestCity->getY()) > 18)
		{
			return DENIAL_NO_GAIN;
		}
	}

	if (getNumCities() > 1 && AI_ourCityValue(pCity) < 600 * getCurrentEra())
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() != getTeam())
	{
		return DENIAL_NEVER;
	}

	if (pCity->calculateCulturePercent(getID()) > 50)
	{
		return DENIAL_TOO_MUCH;
	}
	return NO_DENIAL;
}


int CvPlayerAI::AI_stopTradingTradeVal(TeamTypes eTradeTeam, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_PLAYER(ePlayer).getTeam() != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(eTradeTeam != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_TEAM(eTradeTeam).isAlive(), "GET_TEAM(eWarTeam).isAlive is expected to be true");
	FAssertMsg(!atWar(eTradeTeam, GET_PLAYER(ePlayer).getTeam()), "eTeam should be at peace with eWarTeam");

	int iValue = 50 + GET_TEAM(eTradeTeam).getNumCities() * 5 + GC.getGame().getGameTurn() / 2;

	switch (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).AI_getAttitude(eTradeTeam))
	{
		case ATTITUDE_FURIOUS: break;
		case ATTITUDE_ANNOYED:
		{
			iValue *= 5; iValue /= 4; break;
		}
		case ATTITUDE_CAUTIOUS:
		{
			iValue *= 3; iValue /= 2; break;
		}
		case ATTITUDE_PLEASED:
		{
			iValue *= 2; break;
			break;
		}
		case ATTITUDE_FRIENDLY:
		{
			iValue *= 3; break;
			break;
		}
		default: FErrorMsg("error");
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isOpenBorders(eTradeTeam))
	{
		iValue *= 2;
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isDefensivePact(eTradeTeam))
	{
		iValue *= 3;
	}

	foreach_(CvDeal & kLoopDeal, GC.getGame().deals())
	{
		if (kLoopDeal.isCancelable(getID()) && !kLoopDeal.isPeaceDeal())
		{
			if (GET_PLAYER(kLoopDeal.getFirstPlayer()).getTeam() == GET_PLAYER(ePlayer).getTeam())
			{
				if (kLoopDeal.getLengthSecondTrades() > 0)
				{
					iValue += (GET_PLAYER(kLoopDeal.getFirstPlayer()).AI_dealVal(kLoopDeal.getSecondPlayer(), kLoopDeal.getSecondTrades()) * ((kLoopDeal.getLengthFirstTrades() == 0) ? 2 : 1));
				}
			}

			if (GET_PLAYER(kLoopDeal.getSecondPlayer()).getTeam() == GET_PLAYER(ePlayer).getTeam())
			{
				if (kLoopDeal.getLengthFirstTrades() > 0)
				{
					iValue += (GET_PLAYER(kLoopDeal.getSecondPlayer()).AI_dealVal(kLoopDeal.getFirstPlayer(), kLoopDeal.getFirstTrades()) * ((kLoopDeal.getLengthSecondTrades() == 0) ? 2 : 1));
				}
			}
		}
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDefineINT("DIPLOMACY_VALUE_REMAINDER"));

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDefineINT("DIPLOMACY_VALUE_REMAINDER"));
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_stopTradingTrade(TeamTypes eTradeTeam, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(ePlayer != getID(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_PLAYER(ePlayer).getTeam() != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(eTradeTeam != getTeam(), "shouldn't call this function on ourselves");
	FAssertMsg(GET_TEAM(eTradeTeam).isAlive(), "GET_TEAM(eTradeTeam).isAlive is expected to be true");
	FAssertMsg(!atWar(getTeam(), eTradeTeam), "should be at peace with eTradeTeam");

	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(eTradeTeam))
	{
		return DENIAL_POWER_THEM;
	}

	const AttitudeTypes eAttitude = GET_TEAM(getTeam()).AI_getAttitude(GET_PLAYER(ePlayer).getTeam());

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam())
			{
				if (eAttitude <= GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getStopTradingRefuseAttitudeThreshold())
				{
					return DENIAL_ATTITUDE;
				}
			}
		}
	}

	const AttitudeTypes eAttitudeThem = GET_TEAM(getTeam()).AI_getAttitude(eTradeTeam);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam())
			{
				if (eAttitudeThem > GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getStopTradingThemRefuseAttitudeThreshold())
				{
					return DENIAL_ATTITUDE_THEM;
				}
			}
		}
	}

	if (GET_TEAM(getTeam()).isOpenBorders(eTradeTeam) && GET_TEAM(getTeam()).hasWarPlan(true))
	{
		return DENIAL_MYSTERY;
	}

	return NO_DENIAL;
}


int CvPlayerAI::AI_civicTradeVal(CivicTypes eCivic, PlayerTypes ePlayer) const
{
	int iValue = (2 * (getTotalPopulation() + GET_PLAYER(ePlayer).getTotalPopulation())); // XXX

	const CivicTypes eBestCivic = GET_PLAYER(ePlayer).AI_bestCivic((CivicOptionTypes)(GC.getCivicInfo(eCivic).getCivicOptionType()));

	if (eBestCivic != NO_CIVIC && eBestCivic != eCivic)
	{
		iValue += std::max(0, ((GET_PLAYER(ePlayer).AI_civicValue(eBestCivic) - GET_PLAYER(ePlayer).AI_civicValue(eCivic)) * 2));
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDefineINT("DIPLOMACY_VALUE_REMAINDER"));

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDefineINT("DIPLOMACY_VALUE_REMAINDER"));
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_civicTrade(CivicTypes eCivic, PlayerTypes ePlayer) const
{
	if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
	{
		if (GET_TEAM(getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam()))
		{
			return NO_DENIAL;
		}
	}
	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	if (getCivicPercentAnger(getCivics((CivicOptionTypes)(GC.getCivicInfo(eCivic).getCivicOptionType()))) < getCivicPercentAnger(eCivic))
	{
		return DENIAL_ANGER_CIVIC;
	}

	CivicTypes eFavoriteCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();
	if (eFavoriteCivic != NO_CIVIC
	&& isCivic(eFavoriteCivic)
	&& GC.getCivicInfo(eCivic).getCivicOptionType() == GC.getCivicInfo(eFavoriteCivic).getCivicOptionType())
	{
		return DENIAL_FAVORITE_CIVIC;
	}

	if (GC.getCivilizationInfo(getCivilizationType()).getCivilizationInitialCivics(GC.getCivicInfo(eCivic).getCivicOptionType()) == eCivic)
	{
		return DENIAL_JOKING;
	}

	if (AI_getAttitude(ePlayer) <= GC.getLeaderHeadInfo(getPersonalityType()).getAdoptCivicRefuseAttitudeThreshold())
	{
		return DENIAL_ATTITUDE;
	}

	// Don't change civics when at war (Anarchy is bad)
	if (GET_TEAM(getTeam()).isAtWar(false))
	{
		return DENIAL_JOKING;
	}
	return NO_DENIAL;
}


int CvPlayerAI::AI_religionTradeVal(ReligionTypes eReligion, PlayerTypes ePlayer) const
{
	int iValue = (3 * (getTotalPopulation() + GET_PLAYER(ePlayer).getTotalPopulation())); // XXX

	const ReligionTypes eBestReligion = GET_PLAYER(ePlayer).AI_bestReligion();

	if (eBestReligion != NO_RELIGION && eBestReligion != eReligion)
	{
		iValue += std::max(0, (GET_PLAYER(ePlayer).AI_religionValue(eBestReligion) - GET_PLAYER(ePlayer).AI_religionValue(eReligion)));
	}

	if (GET_TEAM(GET_PLAYER(ePlayer).getTeam()).isVassal(getTeam()))
	{
		iValue /= 2;
	}

	iValue -= (iValue % GC.getDIPLOMACY_VALUE_REMAINDER());

	if (isHumanPlayer())
	{
		return std::max(iValue, GC.getDIPLOMACY_VALUE_REMAINDER());
	}
	return iValue;
}


DenialTypes CvPlayerAI::AI_religionTrade(ReligionTypes eReligion, PlayerTypes ePlayer) const
{
	if (isHumanPlayer())
	{
		return NO_DENIAL;
	}

	if (GET_TEAM(getTeam()).isVassal(GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (atWar(getTeam(), GET_PLAYER(ePlayer).getTeam()))
	{
		return NO_DENIAL;
	}

	if (GET_PLAYER(ePlayer).getTeam() == getTeam())
	{
		return NO_DENIAL;
	}

	if (getStateReligion() != NO_RELIGION)
	{
		if (getHasReligionCount(eReligion) < std::min((getHasReligionCount(getStateReligion()) - 1), (getNumCities() / 2)))
		{
			return DENIAL_MINORITY_RELIGION;
		}
	}

	if (AI_getAttitude(ePlayer) <= GC.getLeaderHeadInfo(getPersonalityType()).getConvertReligionRefuseAttitudeThreshold())
	{
		return DENIAL_ATTITUDE;
	}

	// Don't change religions when at war (Anarchy is bad)
	if (GET_TEAM(getTeam()).isAtWar(false))
	{
		return DENIAL_NO_GAIN;
	}
	return NO_DENIAL;
}

int CvPlayerAI::AI_unitImpassableCount(UnitTypes eUnit) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	foreach_(const TerrainTypes impassableTerrain, GC.getUnitInfo(eUnit).getImpassableTerrains())
	{
		const TechTypes eTech = (TechTypes)GC.getUnitInfo(eUnit).getTerrainPassableTech(impassableTerrain);
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
		{
			iCount++;
		}
	}
	foreach_(const FeatureTypes impassableFeature, GC.getUnitInfo(eUnit).getImpassableFeatures())
	{
		const TechTypes eTech = (TechTypes)GC.getUnitInfo(eUnit).getFeaturePassableTech(impassableFeature);
		if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
		{
			iCount++;
		}
	}

	return iCount;
}

int CvPlayerAI::AI_unitHealerValue(UnitTypes eUnit, UnitCombatTypes eUnitCombat) const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	const int iNumHealUnitCombatTypes = GC.getUnitInfo(eUnit).getNumHealUnitCombatTypes();
	if (eUnitCombat != NO_UNITCOMBAT)
	{
		for (int iI = 0; iI < iNumHealUnitCombatTypes; iI++)
		{
			UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)GC.getUnitInfo(eUnit).getHealUnitCombatType(iI).eUnitCombat;
			if (eHealUnitCombat == eUnitCombat)
			{
				iValue += GC.getUnitInfo(eUnit).getHealUnitCombatType(iI).iHeal;
				iValue += GC.getUnitInfo(eUnit).getHealUnitCombatType(iI).iAdjacentHeal;
			}
		}
	}
	else if (iNumHealUnitCombatTypes > 0)
	{
		int iAverage = 0;
		for (int iI = 0; iI < iNumHealUnitCombatTypes; iI++)
		{
			iAverage += GC.getUnitInfo(eUnit).getHealUnitCombatType(iI).iHeal;
			iAverage += GC.getUnitInfo(eUnit).getHealUnitCombatType(iI).iAdjacentHeal;
		}
		iAverage /= iNumHealUnitCombatTypes;
		iValue += iAverage;
		iValue += iNumHealUnitCombatTypes;
	}

	iValue *= GC.getUnitInfo(eUnit).getNumHealSupport();

	return iValue;
}

int CvPlayerAI::AI_unitPropertyValue(UnitTypes eUnit, PropertyTypes eProperty) const
{
	PROFILE_EXTRA_FUNC();
	const CvPropertyManipulators* propertyManipulators = GC.getUnitInfo(eUnit).getPropertyManipulators();

	if (propertyManipulators)
	{
		int iValue = 0;
		foreach_(const CvPropertySource * pSource, propertyManipulators->getSources())
		{
			if (pSource->getType() == PROPERTYSOURCE_CONSTANT && (eProperty == NO_PROPERTY || pSource->getProperty() == eProperty))
			{
				// Value is crudely, just the AIweight of that property times the source size
				iValue += GC.getPropertyInfo(pSource->getProperty()).getAIWeight() * ((const CvPropertySourceConstant*)pSource)->getAmountPerTurn(getGameObject());
			}
		}
		return iValue;
	}
	return 0;
}

int CvPlayerAI::AI_unitValue(UnitTypes eUnit, UnitAITypes eUnitAI, const CvArea* pArea, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, GC.getNumUnitInfos(), eUnit);
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAI);

	const CvUnitInfo& kUnitInfo = GC.getUnitInfo(eUnit);

	if (kUnitInfo.getDomainType() != AI_unitAIDomainType(eUnitAI) && eUnitAI != UNITAI_ICBM)
	{
		return 0;
	}

	if (kUnitInfo.getNotUnitAIType(eUnitAI) && (criteria == NULL || !criteria->m_bIgnoreNotUnitAIs))
	{
		return 0;
	}

	// Special settler rule
	if (eUnitAI != UNITAI_SETTLE && kUnitInfo.isFound())
	{
		return 0;
	}

	int iGeneralPropertyValue = AI_unitPropertyValue(eUnit);
	bool bisNegativePropertyUnit = (iGeneralPropertyValue < 0);
	bool bisPositivePropertyUnit = (iGeneralPropertyValue > 0);
	bool bUndefinedValid = false, bValid = false;

	switch (eUnitAI)
	{
		case UNITAI_UNKNOWN:
		{
			bUndefinedValid = true;
			break;
		}
		case UNITAI_SUBDUED_ANIMAL:
		{
			bValid = true;
			break;
		}
		case UNITAI_HUNTER:
		{
			if (kUnitInfo.isOnlyDefensive())
			{
				break; // Hard disqualification for hunters.
			}
			// Fall through to next case.
		}
		case UNITAI_HUNTER_ESCORT:
		{
			if (!bisNegativePropertyUnit && kUnitInfo.getCombat() > 0 && kUnitInfo.getMoves() > 0)
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_BARB_CRIMINAL: break;
		case UNITAI_ANIMAL:
		{
			if (isAnimal())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_SETTLE:
		{
			if (kUnitInfo.isFound())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_WORKER:
		{
			if (kUnitInfo.getNumBuilds() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ESCORT:
		{
			if (!bisNegativePropertyUnit && kUnitInfo.getCombat() > 0 && kUnitInfo.getMoves() > 0)//Note: add a hero filter - a lot of them are being trained for this.
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_ATTACK:
		{
			if (kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_CITY:
		{
			if (kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive() && !kUnitInfo.isNoCapture())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_COLLATERAL:
		{
			if (kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive()
			&& (kUnitInfo.getCollateralDamage() > 0 || kUnitInfo.getBreakdownChance() > 0))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PILLAGE:
		{
			if (kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_RESERVE:
		{
			if (!bisNegativePropertyUnit && kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PILLAGE_COUNTER:
		{
			if (kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_COUNTER:
		{
			if (!bisNegativePropertyUnit && kUnitInfo.getCombat() > 0 && !kUnitInfo.isOnlyDefensive())
			{
				if (kUnitInfo.getInterceptionProbability() > 0 || kUnitInfo.getNumTargetUnits() > 0)
				{
					bValid = true;
					break;
				}
				foreach_(const STD_PAIR(UnitTypes, int)& modifier, kUnitInfo.getUnitAttackModifiers())
				{
					if (modifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
				for (int iI = 0; !bValid && iI < GC.getNumUnitCombatInfos(); iI++)
				{
					if (kUnitInfo.getUnitCombatModifier(iI) > 0)
					{
						bValid = true;
						break;
					}

					if (kUnitInfo.getTargetUnitCombat(iI))
					{
						bValid = true;
						break;
					}
				}
				for (int iI = 0; !bValid && iI < GC.getNumUnitInfos(); iI++)
				{
					if (GC.getUnitInfo((UnitTypes)iI).isDefendAgainstUnit(eUnit))
					{
						bValid = true;
						break;
					}

					const int iUnitCombat = kUnitInfo.getUnitCombatType();
					if (NO_UNITCOMBAT != iUnitCombat && GC.getUnitInfo((UnitTypes)iI).getDefenderUnitCombat(iUnitCombat))
					{
						bValid = true;
						break;
					}
				}
			}
			break;
		}
		case UNITAI_HEALER:
		case UNITAI_HEALER_SEA:
		{
			if (!bisNegativePropertyUnit && AI_unitHealerValue(eUnit) > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PROPERTY_CONTROL:
		case UNITAI_PROPERTY_CONTROL_SEA:
		{
			bValid = bisPositivePropertyUnit;
			break;
		}
		case UNITAI_INVESTIGATOR:
		{
			if (kUnitInfo.getInvestigation() > 0 && !bisNegativePropertyUnit)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_INFILTRATOR:
		{
			if (kUnitInfo.isBlendIntoCity())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_SEE_INVISIBLE:
		case UNITAI_SEE_INVISIBLE_SEA:
		{
			if (bisNegativePropertyUnit)
			{
				break; // Not a valid unit for this role
			}
			const InvisibleTypes eVisibilityRequested = criteria ? criteria->m_eVisibility : NO_INVISIBLE;
			if (eVisibilityRequested != NO_INVISIBLE)
			{
				if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
				{
					bool bFound = false;
					for (int iI = 0; iI < kUnitInfo.getNumSeeInvisibleTypes(); ++iI)
					{
						if (kUnitInfo.getSeeInvisibleType(iI) == eVisibilityRequested)
						{
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						break; // Not a valid unit for this role
					}
				}
				else if (kUnitInfo.getVisibilityIntensityType(criteria->m_eVisibility) <= 0)
				{
					break; // Not a valid unit for this role
				}
			}
			else
			{
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
				{
					if (kUnitInfo.getNumVisibilityIntensityTypes() == 0)
					{
						break;  // Not a valid unit for this role
					}
				}
				else if (kUnitInfo.getNumSeeInvisibleTypes() == 0)
				{
					break; // Not a valid unit for this role
				}
			}

			bValid = true;
			break;
		}
		case UNITAI_CITY_DEFENSE:
		{
			if (!bisNegativePropertyUnit && kUnitInfo.getCombat() > 0 && !kUnitInfo.isNoDefensiveBonus())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_CITY_COUNTER:
		{
			if (kUnitInfo.getCombat() > 0 && !bisNegativePropertyUnit && !kUnitInfo.isNoDefensiveBonus())
			{
				if (kUnitInfo.getInterceptionProbability() > 0)
				{
					bValid = true;
					break;
				}

				foreach_(const STD_PAIR(UnitTypes, int)& modifier, kUnitInfo.getUnitDefenseModifiers())
				{
					if (modifier.second > 0)
					{
						bValid = true;
						break;
					}
				}
				for (int iI = 0; !bValid && iI < GC.getNumUnitCombatInfos(); iI++)
				{
					if (kUnitInfo.getUnitCombatModifier(iI) > 0)
					{
						bValid = true;
						break;
					}
				}
			}
			break;
		}
		case UNITAI_CITY_SPECIAL:
		{
			if (!bisNegativePropertyUnit)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_PARADROP:
		{
			if (kUnitInfo.getDropRange() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_EXPLORE:
		{
			if (!bisPositivePropertyUnit && kUnitInfo.getCombat() > 0 && !kUnitInfo.isNoRevealMap())
			{
				bValid = true;
				bUndefinedValid = true;
			}
			break;
		}
		case UNITAI_MISSIONARY:
		{
			if (pArea)
			{
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (kUnitInfo.getReligionSpreads((ReligionTypes)iI) > 0)
					{
						const int iNeededMissionaries = AI_neededMissionaries(pArea, (ReligionTypes)iI);

						if (iNeededMissionaries > 0 && iNeededMissionaries > countReligionSpreadUnits(pArea, (ReligionTypes)iI))
						{
							bValid = true;
							break;
						}
					}
				}
				for (int iI = 0; !bValid && iI < GC.getNumCorporationInfos(); iI++)
				{
					if (kUnitInfo.getCorporationSpreads((CorporationTypes)iI) > 0)
					{
						const int iNeededMissionaries = AI_neededExecutives(pArea, (CorporationTypes)iI);

						if (iNeededMissionaries > 0 && iNeededMissionaries > countCorporationSpreadUnits(pArea, (CorporationTypes)iI))
						{
							bValid = true;
							break;
						}
					}
				}
			}
			break;
		}
		case UNITAI_ICBM:
		{
			if (kUnitInfo.getNukeRange() != -1)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_WORKER_SEA:
		{
			if (kUnitInfo.getNumBuilds() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_SEA:
		{
			if (kUnitInfo.getCombat() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_RESERVE_SEA:
		{
			if (kUnitInfo.getCombat() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ESCORT_SEA:
		{
			if (kUnitInfo.getCombat() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_EXPLORE_SEA:
		{
			if (kUnitInfo.getCombat() > 0 && !(kUnitInfo.isNoRevealMap()))
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ASSAULT_SEA:
		case UNITAI_SETTLER_SEA:
		{
			if (kUnitInfo.getCargoSpace() > 0 && kUnitInfo.getSpecialCargo() == NO_SPECIALUNIT)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_MISSIONARY_SEA:
		case UNITAI_SPY_SEA:
		case UNITAI_CARRIER_SEA:
		case UNITAI_MISSILE_CARRIER_SEA:
		{
			if (kUnitInfo.getCargoSpace() > 0 && kUnitInfo.getSpecialCargo() != NO_SPECIALUNIT)
			{
				for (int i = 0; i < NUM_UNITAI_TYPES; ++i)
				{
					if (GC.getSpecialUnitInfo((SpecialUnitTypes)kUnitInfo.getSpecialCargo()).isCarrierUnitAIType(eUnitAI))
					{
						bValid = true;
						break;
					}
				}
			}
			break;
		}
		case UNITAI_PIRATE_SEA:
		{
			if (kUnitInfo.isAlwaysHostile() && kUnitInfo.isHiddenNationality())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && !kUnitInfo.isSuicide())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_DEFENSE_AIR:
		{
			if (kUnitInfo.getInterceptionProbability() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_CARRIER_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && kUnitInfo.getInterceptionProbability() > 0)
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_MISSILE_AIR:
		{
			if (kUnitInfo.getAirCombat() > 0 && kUnitInfo.isSuicide())
			{
				bValid = true;
			}
			break;
		}
		case UNITAI_ATTACK_CITY_LEMMING:
		case UNITAI_PROPHET:
		case UNITAI_ARTIST:
		case UNITAI_SCIENTIST:
		case UNITAI_GENERAL:
		case UNITAI_GREAT_HUNTER:
		case UNITAI_GREAT_ADMIRAL:
		case UNITAI_MERCHANT:
		case UNITAI_ENGINEER:
		case UNITAI_SPY:
		{
			break;
		}
		default: FErrorMsg("error");
	}

	if (!bValid || !bUndefinedValid && !kUnitInfo.getUnitAIType(eUnitAI))
	{
		return 0;
	}

	PropertyTypes ePropertyRequested = (criteria == NULL ? NO_PROPERTY : criteria->m_eProperty);
	UnitCombatTypes eHealCombatClassRequested = (criteria == NULL ? NO_UNITCOMBAT : criteria->m_eHealUnitCombat);

	const int iCombatValue = GC.getGame().AI_combatValue(eUnit);
	const int iHealerValue = AI_unitHealerValue(eUnit, eHealCombatClassRequested);
	const int iPropertyValue = AI_unitPropertyValue(eUnit, ePropertyRequested);

	int iValue;

	if (ePropertyRequested != NO_PROPERTY && iPropertyValue <= 0)
	{
		iValue = 0;
	}
	else if (eHealCombatClassRequested != NO_UNITCOMBAT && iHealerValue <= 0)
	{
		iValue = 0;
	}
	else
	{
		iValue = 1 + kUnitInfo.getAIWeight();

		switch (eUnitAI)
		{
			case UNITAI_ANIMAL:
			case UNITAI_SUBDUED_ANIMAL:
			case UNITAI_BARB_CRIMINAL:
			{
				break;
			}
			case UNITAI_SETTLE:
			{
				iValue += (kUnitInfo.getMoves() * 100);
				break;
			}
			case UNITAI_WORKER:
			{
				iValue += kUnitInfo.getNumBuilds();
				iValue += kUnitInfo.getMoves() * iValue / 2;
				//	Scale by how fast a worker works - the extra '4' is a fudge factor
				//	to make worker values (somewhat) comparable to military unit values
				//	now that we have workers that can upgrade to military and we need to
				//	compare (at least very roughly)
				iValue = iValue * kUnitInfo.getWorkRate() / 400;
				break;
			}
			case UNITAI_ATTACK:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}


				iValue += iCombatValue;
				{
					const int iFastMoverMultiplier = AI_isDoStrategy(AI_STRATEGY_FASTMOVERS) ? 3 : 1;
					iValue += ((iCombatValue * (kUnitInfo.getMoves() - 1) * iFastMoverMultiplier) / 3);
				}
				iValue += ((iCombatValue * kUnitInfo.getWithdrawalProbability()) / 100);

				if (kUnitInfo.getCombatLimit() < 100)
				{
					iValue -= (iCombatValue * (125 - kUnitInfo.getCombatLimit())) / 100;
				}
				//TB Combat Mods Begin
				if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_DEFENDER_WITHDRAW))
				{
					iValue += ((iCombatValue * kUnitInfo.getPursuit()) / 100);
				}
				if (kUnitInfo.getOverrun() > 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getOverrun()) / 100);
				}
				if (kUnitInfo.getUnyielding() > 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 100);
				}
				if (kUnitInfo.getKnockback() > 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getKnockback()) / 100);
				}
#ifdef BATTLEWORN
				if (kUnitInfo.getStrAdjperRnd() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperRnd()) / 100);
				}
				if (kUnitInfo.getStrAdjperAtt() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperAtt()) / 100);
				}
				if (kUnitInfo.getWithdrawAdjperAtt() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getWithdrawAdjperAtt()) / 100);
				}
#endif // BATTLEWORN
				//TB Combat Mods End
				//	Also useful if attack stacks can make use of defensive terrain, though
				//	its not a huge factor since we can assume some defensive units will be
				//	along for the ride
				if (kUnitInfo.isNoDefensiveBonus())
				{
					iValue *= 4;
					iValue /= 5;
				}

				//	Combat modifiers matter for attack units
				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					iValue += ((iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI)) / 10000);
				}

				break;
			}
			case UNITAI_ATTACK_CITY:
			{
				//	For now the AI cannot cope with bad prroperty values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}

				int iTempValue = ((iCombatValue * iCombatValue) / 75) + (iCombatValue / 2);
				iValue += iTempValue;
				if (kUnitInfo.isNoDefensiveBonus())
				{
					iValue -= iTempValue / 4;
				}

				if (kUnitInfo.isFirstStrikeImmune())
				{
					iValue += (iTempValue * 8) / 100;
				}

				bool bHasBombardValue = false;
				bool bNoBombardValue = true;
				if (kUnitInfo.getBreakdownChance() > 0 || kUnitInfo.getBombardRate() > 0 || (kUnitInfo.getCollateralDamageMaxUnits() > 0 && kUnitInfo.getCollateralDamage() > 0))
				{
					// Army composition needs to scale with army size, bombard unit potency

					//modified AI_calculateTotalBombard(DOMAIN_LAND) code
					int iTotalBombard = 0;
					int iThisBombard = kUnitInfo.getBombardRate();
					int iSiegeUnits = 0;
					int iSiegeImmune = 0;
					int iTotalSiegeMaxUnits = 0;
					bNoBombardValue = false;
					bHasBombardValue = true;

					for (int iJ = 0; iJ < GC.getNumUnitInfos(); iJ++)
					{
						UnitTypes eLoopUnit = (UnitTypes)iJ;
						if (GC.getUnitInfo(eLoopUnit).getDomainType() == DOMAIN_LAND)
						{
							int iUnitCount = getUnitCount(eLoopUnit);
							int iBombardRate = GC.getUnitInfo(eLoopUnit).getBombardRate();
							int iBreakdown = (GC.getUnitInfo(eLoopUnit).getBreakdownChance() * GC.getUnitInfo(eLoopUnit).getBreakdownDamage()) / 10;

							if (iBombardRate > 0)
							{
								iTotalBombard += ((iBombardRate * iUnitCount * ((GC.getUnitInfo(eLoopUnit).isIgnoreBuildingDefense()) ? 3 : 2)) / 2);
							}
							if (iBreakdown > 0)
							{
								iThisBombard += iBreakdown;
								iTotalBombard += ((iBreakdown * iUnitCount * ((GC.getUnitInfo(eLoopUnit).isIgnoreNoEntryLevel()) ? 4 : 2)) / 2);
							}

							int iBombRate = GC.getUnitInfo(eLoopUnit).getBombRate();
							if (iBombRate > 0)
							{
								iThisBombard += iBombRate;
								iTotalBombard += iBombRate * iUnitCount;
							}

							int iCollateralDamageMaxUnits = GC.getUnitInfo(eLoopUnit).getCollateralDamageMaxUnits();
							if (iCollateralDamageMaxUnits > 0 && GC.getUnitInfo(eLoopUnit).getCollateralDamage() > 0)
							{
								iTotalSiegeMaxUnits += iCollateralDamageMaxUnits * iUnitCount;
								iSiegeUnits += iUnitCount;
							}
							else if (GC.getUnitInfo(eLoopUnit).getUnitCombatCollateralImmune((UnitCombatTypes)kUnitInfo.getUnitCombatType()))
							{
								iSiegeImmune += iUnitCount;
							}
						}
					}

					if (iThisBombard == 0)
					{
						bNoBombardValue = true;
					}
					else if ((100 * iTotalBombard) / (std::max(1, (iThisBombard * AI_totalUnitAIs(UNITAI_ATTACK_CITY)))) >= GC.getDefineINT("BBAI_BOMBARD_ATTACK_CITY_MAX_STACK_FRACTION"))
					{
						//too many bombard units already
						bNoBombardValue = true;
					}

					int iNumOffensiveUnits = AI_totalUnitAIs(UNITAI_ATTACK_CITY) + AI_totalUnitAIs(UNITAI_ATTACK) + AI_totalUnitAIs(UNITAI_COUNTER) / 2;
					int iNumDefensiveUnits = AI_totalUnitAIs(UNITAI_CITY_DEFENSE) + AI_totalUnitAIs(UNITAI_RESERVE) + AI_totalUnitAIs(UNITAI_CITY_COUNTER) / 2 + AI_totalUnitAIs(UNITAI_COLLATERAL) / 2;
					iSiegeUnits += (iSiegeImmune * iNumOffensiveUnits) / std::max(1, iNumOffensiveUnits + iNumDefensiveUnits);

					int iMAX_HIT_POINTS = GC.getMAX_HIT_POINTS();

					int iCollateralDamageMaxUnitsWeight = (100 * (iNumOffensiveUnits - iSiegeUnits)) / std::max(1, iTotalSiegeMaxUnits);
					iCollateralDamageMaxUnitsWeight = std::min(100, iCollateralDamageMaxUnitsWeight);
					//to decrease value further for units with low damage limits:
					int iCollateralDamageLimitWeight = 100 * iMAX_HIT_POINTS - std::max(0, ((iMAX_HIT_POINTS - kUnitInfo.getCollateralDamageLimit()) * (100 - iCollateralDamageMaxUnitsWeight)));
					iCollateralDamageLimitWeight /= iMAX_HIT_POINTS;

					int iCollateralValue = iCombatValue * kUnitInfo.getCollateralDamage() * GC.getDefineINT("COLLATERAL_COMBAT_DAMAGE");
					iCollateralValue /= 100;
					iCollateralValue *= std::max(100, (kUnitInfo.getCollateralDamageMaxUnits() * iCollateralDamageMaxUnitsWeight));
					iCollateralValue /= 100;
					iCollateralValue *= iCollateralDamageLimitWeight;
					iCollateralValue /= 100;
					iCollateralValue /= iMAX_HIT_POINTS;
					iValue += iCollateralValue;

					if (!bNoBombardValue && !AI_isDoStrategy(AI_STRATEGY_AIR_BLITZ))
					{
						/* original code
						int iBombardValue = kUnitInfo.getBombardRate() * 4;
						*/
						int iBombardValue = /*kUnitInfo.getBombardRate()*/ iThisBombard * ((kUnitInfo.isIgnoreBuildingDefense() || kUnitInfo.isIgnoreNoEntryLevel()) ? 3 : 2);
						//int iTotalBombardValue = 4 * iTotalBombard;
						//int iNumBombardUnits = 2 * iTotalBombard / iBombardValue;
						int iAIDesiredBombardFraction = std::max(5, GC.getDefineINT("BBAI_BOMBARD_ATTACK_STACK_FRACTION")); /*default: 15*/
						int iActualBombardFraction = (100 * 2 * iTotalBombard) / (iBombardValue * std::max(1, iNumOffensiveUnits));
						iActualBombardFraction = std::min(100, iActualBombardFraction);

						// K - Mod note : This goal has no dependency on civ size, map size, era, strategy, or anything else that matters
						// a flat goal of 200... This needs to be fixed. For now, I'll just replace it with something rough.
						// But this is a future "todo".
						// int iGoalTotalBombard = 200;
						int iGoalTotalBombard = (getNumCities() + 3) * (getCurrentEra() + 2) * (AI_isDoStrategy(AI_STRATEGY_CRUSH) ? 10 : 5);
						int iTempBombardValue = 0;
						if (iTotalBombard < iGoalTotalBombard) //still less than 200 bombard points
						{
							iTempBombardValue = iBombardValue * (iGoalTotalBombard + 7 * (iGoalTotalBombard - iTotalBombard));
							iTempBombardValue /= iGoalTotalBombard;
							//iTempBombardValue is at most (8 * iBombardValue)
						}
						else
						{
							iTempBombardValue *= iGoalTotalBombard;
							iTempBombardValue /= std::min(2 * iGoalTotalBombard, 2 * iTotalBombard - iGoalTotalBombard);
						}

						if (iActualBombardFraction < iAIDesiredBombardFraction)
						{
							iBombardValue *= (iAIDesiredBombardFraction + 5 * (iAIDesiredBombardFraction - iActualBombardFraction));
							iBombardValue /= iAIDesiredBombardFraction;
							//new iBombardValue is at most (6 * old iBombardValue)
						}
						else
						{
							iBombardValue *= iAIDesiredBombardFraction;
							iBombardValue /= std::max(1, iActualBombardFraction);
						}

						if (iTempBombardValue > iBombardValue)
						{
							iBombardValue = iTempBombardValue;
						}
						iBombardValue = getModifiedIntValue(iBombardValue, GC.getDefineINT("C2C_ROUGH_BOMBARD_VALUE_MODIFIER"));

						iValue += iBombardValue;
					}
				}
				//TB Adjust: If the unit doesn't have any bombard value, it can still be beneficial to have collateral damage (Rhinos for example)
				if (!bHasBombardValue)
				{
					iValue += ((iCombatValue * kUnitInfo.getCollateralDamage()) / 200);
				}
				//TB Adjust: If the unit has bombard value(bHasBombardValue) AND the stack still wants bombard units(!bNoBombardValue) (or the unit doesn't have any bombard value anyhow) then basic modifiers apply.
				//This is intended to keep bombarding siege units from evaluating stronger than normal invading units like swordsman for the basic NON-Bombard stack fill needs.
				//Such siege units often can't seal the deal and actually invade the city despite being very necessary for the stack.
				//Before this change we're getting an overbuild of siege units like rams even after the bombard needs are met for the stack.
				if ((bHasBombardValue && !bNoBombardValue) || !bHasBombardValue)
				{
					// Effect army composition to have more collateral/bombard units
					iValue += ((iCombatValue * kUnitInfo.getCityAttackModifier()) / 50);
					{
						const int iFastMoverMultiplier = AI_isDoStrategy(AI_STRATEGY_FASTMOVERS) ? 4 : 1;
						iValue += ((iCombatValue * (kUnitInfo.getMoves() - 1) * iFastMoverMultiplier) / 4); // K-Mod put in -1 !
					}
					iValue += ((iCombatValue * kUnitInfo.getWithdrawalProbability()) / 100);
					//TB Combat Mods Begin

					if (kUnitInfo.getOverrun() > 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getOverrun()) / 80);
					}
					if (kUnitInfo.getUnyielding() > 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 100);
					}
					if (kUnitInfo.getKnockback() > 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getKnockback()) / 80);
					}
#ifdef BATTLEWORN
					if (kUnitInfo.getStrAdjperRnd() != 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getStrAdjperRnd()) / 100);
					}
					if (kUnitInfo.getStrAdjperAtt() != 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getStrAdjperAtt()) / 100);
					}
					if (kUnitInfo.getWithdrawAdjperAtt() != 0)
					{
						iValue += ((iCombatValue * kUnitInfo.getWithdrawAdjperAtt()) / 100);
					}
#endif // BATTLEWORN
				}
				break;
			}
			case UNITAI_COLLATERAL:
			{
				iValue += iCombatValue;
				iValue += ((iCombatValue * kUnitInfo.getCollateralDamage()) / 50);
				iValue += ((iCombatValue * kUnitInfo.getMoves()) / 4);
				iValue += ((iCombatValue * kUnitInfo.getWithdrawalProbability()) / 25);
				//TB Combat Mods Begin
				iValue += ((iCombatValue * kUnitInfo.getWithdrawalProbability() * kUnitInfo.getEarlyWithdraw()) / 25);
				iValue += ((iCombatValue * kUnitInfo.getOverrun()) / 75);
				iValue += ((iCombatValue * kUnitInfo.getKnockback()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getCityAttackModifier()) / 100);// was -= ???
				//TB Combat Mods End
				break;
			}
			case UNITAI_PILLAGE:
			{
				iValue -= AI_unitPropertyValue(eUnit) / 30;	//	Bad properties are good for pillagers
				iValue += iCombatValue;
				iValue += (iCombatValue * kUnitInfo.getMoves());
				iValue += ((iCombatValue * kUnitInfo.getRepel()) / 100);
				break;
			}
			case UNITAI_RESERVE:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += iCombatValue;
				iValue += ((iCombatValue * kUnitInfo.getCollateralDamage()) / 200);
				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					//			int iCombatModifier = kUnitInfo.getUnitCombatModifier(iI);
					//			iCombatModifier = (iCombatModifier < 40) ? iCombatModifier : (40 + (iCombatModifier - 40) / 2);
					//			iValue += ((iCombatValue * iCombatModifier) / 100);
					iValue += ((iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI)) / 12000);
				}
				iValue += ((iCombatValue * kUnitInfo.getMoves()) / 2);
				break;
			}
			case UNITAI_PILLAGE_COUNTER:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				//TB Combat Mods Begin
				iValue += iCombatValue;
				iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getPursuit()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getMoves()) / 2);
				break;
				//TB Combat Mods End
			}
			case UNITAI_COUNTER:
			{
				//	For now the AI cannot cope with bad prroperty values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += (iCombatValue / 2);
				foreach_(const STD_PAIR(UnitTypes, int)& modifier, kUnitInfo.getUnitAttackModifiers())
				{
					iValue += ((iCombatValue * modifier.second * AI_getUnitWeight(modifier.first)) / 7500);
					iValue += ((iCombatValue * (kUnitInfo.isTargetUnit(modifier.first) ? 50 : 0)) / 100);
				}
				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					//			int iCombatModifier = kUnitInfo.getUnitCombatModifier(iI);
					//			iCombatModifier = (iCombatModifier < 40) ? iCombatModifier : (40 + (iCombatModifier - 40) / 2);
					//			iValue += ((iCombatValue * iCombatModifier) / 100);
					iValue += ((iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI)) / 10000);
					iValue += ((iCombatValue * (kUnitInfo.getTargetUnitCombat(iI) ? 50 : 0)) / 100);
				}
				for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
				{
					if (GC.getUnitInfo((UnitTypes)iI).isDefendAgainstUnit(eUnit))
					{
						iValue += (50 * iCombatValue) / 100;
					}

					const int iUnitCombat = kUnitInfo.getUnitCombatType();
					if (NO_UNITCOMBAT != iUnitCombat && GC.getUnitInfo((UnitTypes)iI).getDefenderUnitCombat(iUnitCombat))
					{
						iValue += (50 * iCombatValue) / 100;
					}
				}

				iValue += ((iCombatValue * kUnitInfo.getMoves()) / 2);
				iValue += ((iCombatValue * kUnitInfo.getWithdrawalProbability()) / 100);
				//TB Combat Mods Begin
				iValue += ((iCombatValue * kUnitInfo.getPursuit()) / 150);
				iValue += ((iCombatValue * kUnitInfo.getOverrun()) / 50);
				iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 50);
				//TB Combat Mods End

				if (kUnitInfo.getInterceptionProbability() > 0)
				{
					int iTempValue = kUnitInfo.getInterceptionProbability();

					iTempValue *= 25 + std::min(175, GET_TEAM(getTeam()).AI_getRivalAirPower());
					iTempValue /= 100;

					iValue += iTempValue;
				}
				break;
			}
			case UNITAI_CITY_DEFENSE:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += ((iCombatValue * 2) / 3);
				iValue += ((iCombatValue * kUnitInfo.getCityDefenseModifier()) / 25);
				//	The '30' scaling is empirical based on what seems reasonable for crime fighting units
				// this is causing the AI to select prop control for defense.
				/*iValue += AI_unitPropertyValue(eUnit)/(ePropertyRequested != NO_PROPERTY ? 30 : 60);*/
				//	Combat modifiers matter for defensive units


				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					iValue += iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI) / 12000;
				}

				//  ls612: consider that a unit with OnlyDefensive is less useful

				if (kUnitInfo.isOnlyDefensive())
				{
					iValue *= 4;
					iValue /= 5;
				}

				//TB Combat Mods Begin
				iValue += ((iCombatValue * kUnitInfo.getPursuit()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getRepel()) / 85);
				iValue += ((iCombatValue * kUnitInfo.getFortRepel()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 100);
#ifdef BATTLEWORN
				if (kUnitInfo.getStrAdjperRnd() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperRnd()) / 100);
				}
				if (kUnitInfo.getStrAdjperDef() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperDef()) / 100);
				}
#endif // BATTLEWORN
				//TB Combat Mods End
				break;
			}
			case UNITAI_CITY_COUNTER:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += ((iCombatValue * 2) / 3);
				iValue += ((iCombatValue * kUnitInfo.getCityDefenseModifier()) / 75);
				//	The '30' scaling is empirical based on what seems reasonable for crime fighting units
				// this is causing the AI to select prop control for defense.
				/*iValue += AI_unitPropertyValue(eUnit)/(ePropertyRequested != NO_PROPERTY ? 30 : 60);*/
				//	Combat modifiers matter for defensive units

				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					iValue += iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI) / 6000;
				}
				//  ls612: consider that a unit with OnlyDefensive is less useful

				if (kUnitInfo.isOnlyDefensive())
				{
					iValue *= 4;
					iValue /= 5;
				}

				iValue += ((iCombatValue * kUnitInfo.getPursuit()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getRepel()) / 85);
				iValue += ((iCombatValue * kUnitInfo.getFortRepel()) / 100);
				iValue += ((iCombatValue * kUnitInfo.getUnyielding()) / 100);
#ifdef BATTLEWORN
				if (kUnitInfo.getStrAdjperRnd() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperRnd()) / 100);
				}
				if (kUnitInfo.getStrAdjperDef() != 0)
				{
					iValue += ((iCombatValue * kUnitInfo.getStrAdjperDef()) / 100);
				}
#endif // BATTLEWORN
				break;
			}
			case UNITAI_HEALER:
			case UNITAI_HEALER_SEA:
			{
				iValue += iHealerValue;
				// Drop through
			}
			case UNITAI_PROPERTY_CONTROL:
			case UNITAI_PROPERTY_CONTROL_SEA:
			case UNITAI_CITY_SPECIAL:
			{
				if (iPropertyValue > 0)
				{
					iValue += iPropertyValue * 10 + iCombatValue;
				}
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				else iValue = 0;

				break;
			}
			case UNITAI_PARADROP:
			{
				//	For now the AI cannot cope with bad property values on anything but hunter or pillage units
				if (iPropertyValue < 0)
				{
					iValue = 0;
					break;
				}
				iValue += (iCombatValue / 2);
				iValue += ((iCombatValue * kUnitInfo.getCityDefenseModifier()) / 100);
				iValue /= (kUnitInfo.isOnlyDefensive() ? 2 : 1);
				foreach_(const STD_PAIR(UnitTypes, int)& modifier, kUnitInfo.getUnitAttackModifiers())
				{
					iValue += ((iCombatValue * modifier.second * AI_getUnitWeight(modifier.first)) / 10000);
					iValue += ((iCombatValue * (kUnitInfo.isDefendAgainstUnit(modifier.first) ? 50 : 0)) / 100);
				}
				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					iValue += ((iCombatValue * kUnitInfo.getUnitCombatModifier(iI) * AI_getUnitCombatWeight((UnitCombatTypes)iI)) / 10000);
					iValue += ((iCombatValue * (kUnitInfo.getDefenderUnitCombat(iI) ? 50 : 0)) / 100);
				}

				if (kUnitInfo.getInterceptionProbability() > 0)
				{
					int iTempValue = kUnitInfo.getInterceptionProbability();

					iTempValue *= 25 + std::min(125, GET_TEAM(getTeam()).AI_getRivalAirPower());
					iTempValue /= 50;

					iValue += iTempValue;
				}
				break;
			}
			case UNITAI_EXPLORE:
			{
				iValue += kUnitInfo.getMoves() * kUnitInfo.getMoves() * (100 + iCombatValue) / 4;
				if (kUnitInfo.isNoBadGoodies())
				{
					iValue *= 2;
				}
				//need to add vision and terrain factors here.
				break;
			}
			case UNITAI_HUNTER:
			{
				iValue += iCombatValue * kUnitInfo.getMoves();
				iValue = (
					getModifiedIntValue(
						iValue,
						  kUnitInfo.getPursuit()
						+ kUnitInfo.getAnimalCombatModifier()
						+ kUnitInfo.getUnitCombatModifier(GC.getUNITCOMBAT_ANIMAL())
						+ kUnitInfo.getPursuitVSUnitCombatType(GC.getUNITCOMBAT_ANIMAL())
					)
				);

				if (kUnitInfo.hasUnitCombat(GC.getUNITCOMBAT_HUNTER()))
				{
					iValue = iValue * 3/2; // Unique hunter promotions are essential.
				}
				break;
			}
			case UNITAI_HUNTER_ESCORT:
			{
				iValue += iCombatValue * kUnitInfo.getMoves();
				break;
			}
			case UNITAI_MISSIONARY:
			{
				iValue += (kUnitInfo.getMoves() * 100);
				if (getStateReligion() != NO_RELIGION)
				{
					if (kUnitInfo.getReligionSpreads(getStateReligion()) > 0)
					{
						iValue += (5 * kUnitInfo.getReligionSpreads(getStateReligion())) / 2;
					}
				}
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if (kUnitInfo.getReligionSpreads((ReligionTypes)iI) && hasHolyCity((ReligionTypes)iI))
					{
						iValue += 80;
						break;
					}
				}

				if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
				{
					int iTempValue = 0;
					for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
					{
						if (kUnitInfo.getReligionSpreads((ReligionTypes)iI))
						{
							iTempValue += (50 * getNumCities()) / (1 + getHasReligionCount((ReligionTypes)iI));
						}
					}
					iValue += iTempValue;
				}
				for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
				{
					if (hasHeadquarters((CorporationTypes)iI) && kUnitInfo.getCorporationSpreads(iI) > 0)
					{
						iValue += kUnitInfo.getCorporationSpreads(iI) * 5/2;

						if (pArea)
						{
							iValue += 300 / std::max(1, pArea->countHasCorporation((CorporationTypes)iI, getID()));
						}
					}
				}
				break;
			}
			case UNITAI_ICBM:
			{
				if (kUnitInfo.getNukeRange() != -1)
				{
					int iTempValue = 40 + (kUnitInfo.getNukeRange() * 40);
					if (kUnitInfo.getAirRange() == 0)
					{
						iValue += iTempValue;
					}
					else
					{
						iValue += (iTempValue * std::min(10, kUnitInfo.getAirRange())) / 10;
					}
					iValue += (iTempValue * (60 + kUnitInfo.getEvasionProbability())) / 100;
				}
				break;
			}
			case UNITAI_WORKER_SEA:
			{
				iValue += 50 * kUnitInfo.getNumBuilds();
				iValue += kUnitInfo.getMoves() * 100;
				break;
			}
			case UNITAI_ATTACK_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * kUnitInfo.getMoves() / 2;
				iValue += kUnitInfo.getBombardRate() * 4;
				break;
			}
			case UNITAI_RESERVE_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * kUnitInfo.getMoves();
				break;
			}
			case UNITAI_ESCORT_SEA:
			{
				iValue += iCombatValue;
				iValue += iCombatValue * kUnitInfo.getMoves();
				iValue += kUnitInfo.getInterceptionProbability() * 3;
				if (kUnitInfo.getNumSeeInvisibleTypes() > 0)
				{
					iValue += 200;
				}
				// Boats which can't be seen don't play defense, don't make good escorts
				if (kUnitInfo.getInvisibleType() != NO_INVISIBLE)
				{
					iValue /= 2;
				}
				break;
			}
			case UNITAI_EXPLORE_SEA:
			{
				int iExploreValue = 100;
				if (pArea)
				{
					if (pArea->isWater())
					{
						if (pArea->getUnitsPerPlayer(BARBARIAN_PLAYER) > 0)
						{
							iExploreValue += (2 * iCombatValue);
						}
					}
				}
				iValue += (kUnitInfo.getMoves() * iExploreValue);
				if (kUnitInfo.isAlwaysHostile())
				{
					iValue /= 2;
				}
				iValue /= (1 + AI_unitImpassableCount(eUnit));
				break;
			}
			case UNITAI_ASSAULT_SEA:
			case UNITAI_SETTLER_SEA:
			case UNITAI_MISSIONARY_SEA:
			case UNITAI_SPY_SEA:
			{
				iValue += (iCombatValue / 2);
				iValue += (kUnitInfo.getMoves() * 200);
				iValue += (kUnitInfo.getCargoSpace() * 300);
				// Never build galley transports when ocean faring ones exist (issue mainly for Carracks)
				iValue /= (1 + AI_unitImpassableCount(eUnit));
				break;
			}
			case UNITAI_CARRIER_SEA:
			{
				iValue += iCombatValue;
				iValue += (kUnitInfo.getMoves() * 50);
				iValue += (kUnitInfo.getCargoSpace() * 400);
				break;
			}
			case UNITAI_MISSILE_CARRIER_SEA:
			{
				iValue += iCombatValue * kUnitInfo.getMoves();
				iValue += (25 + iCombatValue) * (3 + (kUnitInfo.getCargoSpace()));
				break;
			}
			case UNITAI_PIRATE_SEA:
			{
				iValue += iCombatValue;
				iValue += (iCombatValue * kUnitInfo.getMoves());
				break;
			}
			case UNITAI_ATTACK_AIR:
			{
				iValue += iCombatValue;
				iValue += (kUnitInfo.getCollateralDamage() * iCombatValue) / 100;
				iValue += 4 * kUnitInfo.getBombRate();
				iValue += (iCombatValue * (100 + 2 * kUnitInfo.getCollateralDamage()) * kUnitInfo.getAirRange()) / 100;
				break;
			}
			case UNITAI_DEFENSE_AIR:
			{
				iValue += iCombatValue;
				iValue += (kUnitInfo.getInterceptionProbability() * 3);
				iValue += (kUnitInfo.getAirRange() * iCombatValue);
				break;
			}
			case UNITAI_CARRIER_AIR:
			{
				iValue += (iCombatValue);
				iValue += (kUnitInfo.getInterceptionProbability() * 2);
				iValue += (kUnitInfo.getAirRange() * iCombatValue);
				break;
			}
			case UNITAI_MISSILE_AIR:
			{
				iValue += iCombatValue;
				iValue += 4 * kUnitInfo.getBombRate();
				iValue += kUnitInfo.getAirRange() * iCombatValue;
				break;
			}
			case UNITAI_INVESTIGATOR:
			{
				iValue += iCombatValue;
				iValue *= kUnitInfo.getInvestigation();
				break;
			}
			case UNITAI_INFILTRATOR:
			{
				iValue += iCombatValue;
				iValue *= kUnitInfo.getInsidiousness();
				if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
				{
					for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
					{
						iValue += kUnitInfo.getInvisibilityIntensityType(iI) * 10;
					}
				}
				else
				{
					if (kUnitInfo.getInvisibleType() != NO_INVISIBLE)
					{
						iValue *= 10;
					}
					else
					{
						iValue = 0;
					}
				}
				//crime levels and many other things COULD factor in here but most all of those things rank up as these details do so this should be sufficient
				break;
			}
			case UNITAI_SEE_INVISIBLE:
			case UNITAI_SEE_INVISIBLE_SEA:
			{
				const InvisibleTypes eVisibilityRequested = criteria ? criteria->m_eVisibility : NO_INVISIBLE;
				iValue += iCombatValue;
				iValue += kUnitInfo.getMoves() * 3;
				if (eVisibilityRequested != NO_INVISIBLE)
				{
					if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
					{
						iValue *= kUnitInfo.getVisibilityIntensityType(eVisibilityRequested);
					}
					else
					{
						for (int iI = 0; iI < kUnitInfo.getNumSeeInvisibleTypes(); ++iI)
						{
							if (kUnitInfo.getSeeInvisibleType(iI) == (int)eVisibilityRequested)
							{
								iValue *= 100;
								break;
							}
						}
					}
				}
				else
				{
					if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
					{
						const InvisibilityArray& array = kUnitInfo.getVisibilityIntensityTypes();
						for (InvisibilityArray::const_iterator it = array.begin(); it != array.end(); ++it)
						{
							iValue = getModifiedIntValue(iValue, 10 * (*it).second);
						}
					}
					else
					{
						iValue += 50 * kUnitInfo.getNumSeeInvisibleTypes();
					}
				}
				break;
			}
			case UNITAI_ESCORT:
			{
				iValue += iCombatValue;
				//for every 10 pts of combat value, make each move pt count for 1 more than a base 1 each.
				iValue += kUnitInfo.getMoves() * (1 + iCombatValue / 10);
				//Combat weaknesses are very bad
				for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
				{
					const int iTerrainModifier = kUnitInfo.getTerrainDefenseModifier(iI);
					if (iTerrainModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iTerrainModifier);
					}
					else if (iTerrainModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iTerrainModifier / 5;
					}
				}
				for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
				{
					const int iFeatureModifier = kUnitInfo.getFeatureDefenseModifier(iI);
					if (iFeatureModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iFeatureModifier);
					}
					else if (iFeatureModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iFeatureModifier / 5;
					}
				}
				for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
				{
					const int iCombatModifier = kUnitInfo.getUnitCombatModifier(iI);
					if (iCombatModifier < 0)
					{
						iValue = getModifiedIntValue(iValue, iCombatModifier);
					}
					else if (iCombatModifier > 0)
					{
						//Strengths are good but not to be too swaying or hunter types and others with grave weaknesses will be selected.
						iValue += iCombatModifier / 5;
					}
				}
				//General defense, if the unit has it, is very good. Very bad if penalized.
				iValue = getModifiedIntValue(iValue, kUnitInfo.getDefenseCombatModifier());

				if (kUnitInfo.isNoDefensiveBonus())
				{
					iValue /= 4;
				}
				if (kUnitInfo.isOnlyDefensive())
				{
					iValue *= 3;
					iValue /= 2;
					//better because it enables the unit to move through opponent territory with a RoP
				}
				break;
			}
			default: FErrorMsg("error");
		}
	}

	if (iCombatValue > 0 && pArea != NULL && (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY))
	{
		const AreaAITypes eAreaAI = pArea->getAreaAIType(getTeam());
		if (eAreaAI == AREAAI_ASSAULT || eAreaAI == AREAAI_ASSAULT_MASSING)
		{
			for (int iI = 0; iI < GC.getNumPromotionInfos(); iI++)
			{
				if (kUnitInfo.getFreePromotions(iI) && GC.getPromotionInfo((PromotionTypes)iI).isAmphib())
				{
					iValue *= 133;
					iValue /= 100;
					break;
				}
			}
		}
	}
	return std::max(0, iValue);
}


int CvPlayerAI::AI_totalUnitAIs(UnitAITypes eUnitAI) const
{
	return (AI_getNumTrainAIUnits(eUnitAI) + AI_getNumAIUnits(eUnitAI));
}


int CvPlayerAI::AI_totalAreaUnitAIs(const CvArea* pArea, UnitAITypes eUnitAI) const
{
	return (pArea->getNumTrainAIUnits(getID(), eUnitAI) + pArea->getNumAIUnits(getID(), eUnitAI));
}


int CvPlayerAI::AI_totalWaterAreaUnitAIs(const CvArea* pArea, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = AI_totalAreaUnitAIs(pArea, eUnitAI);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			foreach_(const CvCity * pLoopCity, GET_PLAYER((PlayerTypes)iI).cities())
			{
				if (pLoopCity->waterArea() == pArea)
				{
					iCount += pLoopCity->plot()->plotCount(PUF_isUnitAIType, eUnitAI, -1, NULL, getID());

					if (pLoopCity->getOwner() == getID())
					{
						iCount += pLoopCity->getNumTrainUnitAI(eUnitAI);
					}
				}
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_countCargoSpace(UnitAITypes eUnitAI) const
{
	return algo::accumulate(units()
		| filtered(CvUnit::fn::AI_getUnitAIType() == eUnitAI)
		| transformed(CvUnit::fn::cargoSpace()), 0);
}


int CvPlayerAI::AI_neededExplorers(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pArea != NULL);
	int iNeeded = (
		std::min(
			(100 + pArea->getNumTiles())
			*
			(1 + pArea->getNumUnrevealedTiles(getTeam()) + pArea->getNumUnownedTiles())
			/
			(100 * pArea->getNumTiles()),
			// Limit the need for very big land based on empire size.
			std::max(5, 3 + getNumCities() / 3)
		)
	);

	if (0 == iNeeded && GC.getGame().countCivTeamsAlive() - 1 > GET_TEAM(getTeam()).getHasMetCivCount(true))
	{
		if (pArea->isWater())
		{
			if (GC.getMap().findBiggestArea(true) == pArea)
			{
				iNeeded++;
			}
		}
		else if (getCapitalCity() != NULL && pArea->getID() == getCapitalCity()->getArea())
		{
			for (int iPlayer = 0; iPlayer < MAX_PC_PLAYERS; iPlayer++)
			{
				const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kPlayer.isAlive() && kPlayer.getTeam() != getTeam() && !GET_TEAM(getTeam()).isHasMet(kPlayer.getTeam()))
				{
					if (pArea->getCitiesPerPlayer(kPlayer.getID()) > 0)
					{
						iNeeded++;
						break;
					}
				}
			}
		}
	}
	return iNeeded;
}

int CvPlayerAI::AI_neededHunters(const CvArea* pArea) const
{
	FAssert(pArea);

	if (pArea->isWater())
	{
		return 0; // Hunter AI currently only operates on land
	}

	if (GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT) && pArea->getNumUnownedTiles() == 0)
	{
		return 1; // A hunter unit might come in handy at some point
	}
	return (
		std::min(
			getNumCities() / 2 + pArea->getNumUnownedTiles() / 16 + 1,
			getNumCities() + pArea->getNumUnownedTiles() / 64)
	);
}


int CvPlayerAI::AI_neededWorkers(const CvArea* pArea) const
{
	PROFILE_EXTRA_FUNC();
	int iNeeded = 0;
	int iCities = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->getArea() == pArea->getID())
		{
			iNeeded += pLoopCity->AI_getWorkersNeeded();
			iCities++;
		}
	}
	if (iNeeded == 0)
	{
		return 0;
	}
	iNeeded = std::max(iNeeded, 1 + iCities + intSqrt(iCities)); // max 1 + 1 workers per city in area + 1 worker per city squared in area.

	if (gPlayerLogLevel > 2)
	{
		logBBAI("Player %d needs %d Workers in Area %d.\n", getID(), iNeeded, pArea->getID());
	}
	return iNeeded;

}


int CvPlayerAI::AI_neededMissionaries(const CvArea* pArea, ReligionTypes eReligion) const
{
	PROFILE_FUNC();

	const bool bCultureVictory = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2);
	const bool bHoly = hasHolyCity(eReligion);
	const bool bState = (getStateReligion() == eReligion);
	const bool bHolyState = ((getStateReligion() != NO_RELIGION) && hasHolyCity(getStateReligion()));

	int iInternalCount = 0;
	int iExternalCount = 0;
	bool bReligiousVictory = false;
	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		bReligiousVictory = true;
	}

	//internal spread.
	if ((bCultureVictory || bState || bHoly)
	&& !(!bState && bReligiousVictory))
	{
		iInternalCount = std::max(iInternalCount, (pArea->getCitiesPerPlayer(getID()) - pArea->countHasReligion(eReligion, getID())));
		if (iInternalCount > 0)
		{
			if (!bCultureVictory && !bReligiousVictory)
			{
				iInternalCount = std::max(1, iInternalCount / (bHoly ? 2 : 4));
			}
		}
	}

	//external spread.
	if (((bHoly && bState) || (bHoly && !bHolyState && (getStateReligion() != NO_RELIGION)))
	&& !(!bState && bReligiousVictory))
	{
		if (bState && bReligiousVictory)
		{
			iExternalCount += (pArea->getNumCities() - pArea->countHasReligion(eReligion));
		}
		else
		{
			iExternalCount += ((pArea->getNumCities() * 2) - (pArea->countHasReligion(eReligion) * 3));
		}

		if (bState && bReligiousVictory)
		{
			if (isConsiderReligiousVictory())
			{
				iExternalCount /= 3;
			}
		}
		else
		{
			iExternalCount /= 8;
		}

		iExternalCount = std::max(0, iExternalCount);

		if (AI_isPrimaryArea(pArea))
		{
			iExternalCount++;
		}
	}
	int iCount = iExternalCount + iInternalCount;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (GC.getGame().canEverTrain((UnitTypes)iI)
		&& GC.getUnitInfo((UnitTypes)iI).getDefaultUnitAIType() == UNITAI_MISSIONARY
		&& GC.getUnitInfo((UnitTypes)iI).getAdvisorType() == 1)
		{
			iCount -= getUnitCountPlusMaking((UnitTypes)iI);
		}
	}
	iCount = std::max(0, iCount);
	return iCount;
}


int CvPlayerAI::AI_neededExecutives(const CvArea* pArea, CorporationTypes eCorporation) const
{
	if (!hasHeadquarters(eCorporation))
	{
		return 0;
	}

	int iCount = ((pArea->getCitiesPerPlayer(getID()) - pArea->countHasCorporation(eCorporation, getID())) * 2);
	iCount += (pArea->getNumCities() - pArea->countHasCorporation(eCorporation));

	iCount /= 3;

	if (AI_isPrimaryArea(pArea))
	{
		++iCount;
	}

	return iCount;
}

//Looks like this is an expression of the amount of supporting (same player's) attackers available adjacent to the plot in question
int CvPlayerAI::AI_adjacentPotentialAttackers(const CvPlot* pPlot, bool bTestCanMove) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	foreach_(const CvPlot * pLoopPlot, pPlot->adjacent() | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->getOwner() == getID() && pLoopUnit->getDomainType() == (pPlot->isWater() ? DOMAIN_SEA : DOMAIN_LAND))
			{
				if (pLoopUnit->canAttack() && (!bTestCanMove || pLoopUnit->canMove()) && !pLoopUnit->AI_isCityAIType())
				{
					iCount++;
				}
			}
		}
	}

	return iCount;
}

//TB tooling around - this may at some point be useful...
//int CvPlayerAI::AI_PotentialEnemyAttackers(CvPlot* pPlot, bool bTestCanMove, bool bTestVisible) const
//{
//	int iCount = 0;
//
//	foreach_(CvUnit* pLoopUnit, pPlot->units())
//	{
//		if (GET_TEAM(getTeam()).isAtWar(pLoopUnit->getTeam()))
//		{
//			if (pLoopUnit->getDomainType() == ((pPlot->isWater()) ? DOMAIN_SEA : DOMAIN_LAND) || pLoopUnit->canMoveAllTerrain())
//			{
//				if (pLoopUnit->canAttack())
//				{
//					if (!bTestCanMove || pLoopUnit->canMove())
//					{
//						if (!bTestVisible || (bTestVisible && !(pLoopUnit->isInvisible(getTeam(), true));
//						{
//							if (!(pLoopUnit->AI_isCityAIType()))
//							{
//								iCount++;
//							}
//						}
//					}
//				}
//			}
//		}
//	}
//
//	return iCount;
//}
//
//int CvPlayerAI::AI_PotentialDefenders(CvPlot* pPlot, bool bTestVisible) const
//{
//	int iCount = 0;
//
//	foreach_(const CvUnit* pLoopUnit, pPlot->units())
//	{
//		if (GET_TEAM(getTeam()).isAtWar(pLoopUnit->getTeam()))
//		{
//			if (!bTestVisible || (bTestVisible && !(pLoopUnit->isInvisible(getTeam(), false));
//			{
//				if (pLoopUnit->canDefend())
//				{
//					iCount++;
//				}
//			}
//		}
//	}
//
//	return iCount;
//}


int CvPlayerAI::AI_totalMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			if (pLoopSelectionGroup->AI_getMissionAIType() == eMissionAI)
			{
				iCount += pLoopSelectionGroup->getNumUnits();
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_missionaryValue(const CvArea* pArea, ReligionTypes eReligion, PlayerTypes* peBestPlayer) const
{
	PROFILE_EXTRA_FUNC();
	CvTeam& kTeam = GET_TEAM(getTeam());
	CvGame& kGame = GC.getGame();

	int iSpreadInternalValue = 100;
	int iSpreadExternalValue = 0;

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  03/08/10								jdog5000	  */
	/*																							  */
	/* Victory Strategy AI																		  */
	/************************************************************************************************/
		// Obvious copy & paste bug
	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
	{
		iSpreadInternalValue += 500;
		if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
		{
			iSpreadInternalValue += 1500;
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
			{
				iSpreadInternalValue += 3000;
			}
		}
	}
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/


	/************************************************************************************************/
	/* RevDCM					  Start		 5/1/09												 */
	/*																							  */
	/* Inquisitions																				 */
	/************************************************************************************************/
	bool bStateReligion = (getStateReligion() == eReligion);
	bool bReligiousVictory = false;
	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		bReligiousVictory = true;
	}

	if (!bStateReligion && bReligiousVictory)
	{
		return 0;
	}

	if (bStateReligion && bReligiousVictory)
	{
		if (isPushReligiousVictory())
		{
			iSpreadInternalValue += 2500;
		}
		else
		{
			iSpreadInternalValue += 700;
		}
	}
	/************************************************************************************************/
	/* Inquisitions						 END														*/
	/************************************************************************************************/

	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					  10/03/09								jdog5000	  */
	/*																							  */
	/* Missionary AI																				*/
	/************************************************************************************************/
		// In free religion, treat all religions like state religions

	if (!isStateReligion())
	{
		// Free religion
		iSpreadInternalValue += 500;
		bStateReligion = true;
	}
	else if (bStateReligion)
	{
		iSpreadInternalValue += 1000;
	}
	else
	{
		iSpreadInternalValue += (500 * getHasReligionCount(eReligion)) / std::max(1, getNumCities());
	}

	int iGoldValue = 0;
	if (kTeam.hasHolyCity(eReligion))
	{
		iSpreadInternalValue += bStateReligion ? 1000 : 300;
		iSpreadExternalValue += bStateReligion ? 1000 : 150;
		if (kTeam.hasShrine(eReligion))
		{
			iSpreadInternalValue += bStateReligion ? 500 : 300;
			iSpreadExternalValue += bStateReligion ? 300 : 200;
			const int iGoldMultiplier = kGame.getHolyCity(eReligion)->getTotalCommerceRateModifier(COMMERCE_GOLD);
			iGoldValue = 6 * iGoldMultiplier;
		}
	}

	int iOurCitiesHave = 0;
	int iOurCitiesCount = 0;

	if (NULL == pArea)
	{
		iOurCitiesHave = kTeam.getHasReligionCount(eReligion);
		iOurCitiesCount = kTeam.getNumCities();
	}
	else
	{
		iOurCitiesHave = pArea->countHasReligion(eReligion, getID()) + countReligionSpreadUnits(pArea, eReligion, true);
		iOurCitiesCount = pArea->getCitiesPerPlayer(getID());
	}

	if (iOurCitiesHave < iOurCitiesCount)
	{
		iSpreadInternalValue *= 30 + ((100 * (iOurCitiesCount - iOurCitiesHave)) / iOurCitiesCount);
		iSpreadInternalValue /= 100;
		iSpreadInternalValue += iGoldValue;
	}
	else
	{
		iSpreadInternalValue = 0;
	}

	if (iSpreadExternalValue > 0)
	{
		int iBestPlayer = NO_PLAYER;
		int iBestValue = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
		{
			if (iPlayer != getID())
			{
				CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() != getTeam() && kLoopPlayer.getNumCities() > 0)
				{
					/************************************************************************************************/
					/* Afforess					  Start		 12/9/09												*/
					/*																							  */
					/*																							  */
					/************************************************************************************************/
					if (GET_TEAM(kLoopPlayer.getTeam()).isOpenBorders(getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isLimitedBorders(getTeam()))
						/************************************************************************************************/
						/* Afforess						 END															*/
						/************************************************************************************************/
					{
						int iCitiesCount = 0;
						int iCitiesHave = 0;
						int iMultiplier = AI_isDoStrategy(AI_STRATEGY_MISSIONARY) ? 60 : 25;
						if (!kLoopPlayer.isNoNonStateReligionSpread() || (kLoopPlayer.getStateReligion() == eReligion))
						{
							if (NULL == pArea)
							{
								iCitiesCount += 1 + (kLoopPlayer.getNumCities() * 75) / 100;
								iCitiesHave += std::min(iCitiesCount, kLoopPlayer.getHasReligionCount(eReligion));
							}
							else
							{
								int iPlayerSpreadPercent = (100 * kLoopPlayer.getHasReligionCount(eReligion)) / kLoopPlayer.getNumCities();
								iCitiesCount += pArea->getCitiesPerPlayer((PlayerTypes)iPlayer);
								iCitiesHave += std::min(iCitiesCount, (iCitiesCount * iPlayerSpreadPercent) / 75);
							}
						}

						if (kLoopPlayer.getStateReligion() == NO_RELIGION)
						{
							// Paganism counts as a state religion civic, that's what's caught below
							if (kLoopPlayer.getStateReligionCount() > 0)
							{
								const int iTotalReligions = kLoopPlayer.countTotalHasReligion();
								iMultiplier += 100 * std::max(0, kLoopPlayer.getNumCities() - iTotalReligions);
								iMultiplier += (iTotalReligions == 0) ? 100 : 0;
							}
						}

						int iValue = (iMultiplier * iSpreadExternalValue * (iCitiesCount - iCitiesHave)) / std::max(1, iCitiesCount);
						iValue /= 100;
						iValue += iGoldValue;

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							iBestPlayer = iPlayer;
						}
					}
				}
			}
		}

		if (iBestValue > iSpreadInternalValue)
		{
			if (NULL != peBestPlayer)
			{
				*peBestPlayer = (PlayerTypes)iBestPlayer;
			}
			return iBestValue;
		}

	}

	if (NULL != peBestPlayer)
	{
		*peBestPlayer = getID();
	}
	return iSpreadInternalValue;
	/************************************************************************************************/
	/* BETTER_BTS_AI_MOD					   END												  */
	/************************************************************************************************/
}

int CvPlayerAI::AI_executiveValue(const CvArea* pArea, CorporationTypes eCorporation, PlayerTypes* peBestPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvTeam& kTeam = GET_TEAM(getTeam());
	const CvGame& kGame = GC.getGame();

	int iSpreadInternalValue = 100;
	int iSpreadExternalValue = 0;

	if (kTeam.hasHeadquarters(eCorporation))
	{
		int iGoldMultiplier = kGame.getHeadquarters(eCorporation)->getTotalCommerceRateModifier(COMMERCE_GOLD);
		iSpreadInternalValue += 10 * std::max(0, (iGoldMultiplier - 100));
		iSpreadExternalValue += 15 * std::max(0, (iGoldMultiplier - 150));
	}

	int iOurCitiesHave = 0;
	int iOurCitiesCount = 0;

	if (NULL == pArea)
	{
		iOurCitiesHave = kTeam.getHasCorporationCount(eCorporation);
		iOurCitiesCount = kTeam.getNumCities();
	}
	else
	{
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					  11/14/09								jdog5000	  */
		/*																							  */
		/* City AI																					  */
		/************************************************************************************************/
		iOurCitiesHave = pArea->countHasCorporation(eCorporation, getID()) + countCorporationSpreadUnits(pArea, eCorporation, true);
		/************************************************************************************************/
		/* BETTER_BTS_AI_MOD					   END												  */
		/************************************************************************************************/
		iOurCitiesCount = pArea->getCitiesPerPlayer(getID());
	}

	for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); iCorp++)
	{
		if (kGame.isCompetingCorporation(eCorporation, (CorporationTypes)iCorp))
		{
			if (NULL == pArea)
			{
				iOurCitiesHave += kTeam.getHasCorporationCount(eCorporation);
			}
			else
			{
				iOurCitiesHave += pArea->countHasCorporation(eCorporation, getID());
			}
		}
	}

	if (iOurCitiesHave >= iOurCitiesCount)
	{
		iSpreadInternalValue = 0;
		/************************************************************************************************/
		/* UNOFFICIAL_PATCH					   06/23/10								  denev	   */
		/*																							  */
		/* Bugfix																					   */
		/************************************************************************************************/
		/* original bts code
				if (iSpreadExternalValue = 0)
		*/
		if (iSpreadExternalValue == 0)
			/************************************************************************************************/
			/* UNOFFICIAL_PATCH						END												  */
			/************************************************************************************************/
		{
			return 0;
		}
	}

	int iBonusValue = 0;
	CvCity* pCity = getCapitalCity();
	if (pCity != NULL)
	{
		iBonusValue = AI_corporationValue(eCorporation, pCity);
		iBonusValue /= 25;
	}

	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.isAlive() && (kLoopPlayer.getNumCities() > 0))
		{
			if ((kLoopPlayer.getTeam() == getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isVassal(getTeam()))
			{
				if (kLoopPlayer.getHasCorporationCount(eCorporation) == 0)
				{
					iBonusValue += 1000;
				}
			}
		}
	}

	if (iBonusValue == 0)
	{
		return 0;
	}

	iSpreadInternalValue += iBonusValue;

	if (iSpreadExternalValue > 0)
	{
		int iBestPlayer = NO_PLAYER;
		int iBestValue = 0;
		for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
		{
			if (iPlayer != getID())
			{
				CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
				if (kLoopPlayer.isAlive() && (kLoopPlayer.getTeam() != getTeam()) && (kLoopPlayer.getNumCities() > 0))
				{
					/************************************************************************************************/
					/* Afforess					  Start		 12/9/09												*/
					/*																							  */
					/*																							  */
					/************************************************************************************************/
					if (GET_TEAM(kLoopPlayer.getTeam()).isOpenBorders(getTeam()) || GET_TEAM(kLoopPlayer.getTeam()).isLimitedBorders(getTeam()))
						/************************************************************************************************/
						/* Afforess						 END															*/
						/************************************************************************************************/
					{
						if (!kLoopPlayer.isNoCorporations() && !kLoopPlayer.isNoForeignCorporations())
						{
							int iCitiesCount = 0;
							int iCitiesHave = 0;
							int iMultiplier = AI_getAttitudeWeight((PlayerTypes)iPlayer);
							if (NULL == pArea)
							{
								iCitiesCount += 1 + (kLoopPlayer.getNumCities() * 50) / 100;
								iCitiesHave += std::min(iCitiesCount, kLoopPlayer.getHasCorporationCount(eCorporation));
							}
							else
							{
								int iPlayerSpreadPercent = (100 * kLoopPlayer.getHasCorporationCount(eCorporation)) / kLoopPlayer.getNumCities();
								iCitiesCount += pArea->getCitiesPerPlayer((PlayerTypes)iPlayer);
								iCitiesHave += std::min(iCitiesCount, (iCitiesCount * iPlayerSpreadPercent) / 50);
							}

							if (iCitiesHave < iCitiesCount)
							{
								int iValue = (iMultiplier * iSpreadExternalValue);
								iValue += ((iMultiplier - 55) * iBonusValue) / 4;
								iValue /= 100;
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									iBestPlayer = iPlayer;
								}
							}
						}
					}
				}
			}
		}

		if (iBestValue > iSpreadInternalValue)
		{
			if (NULL != peBestPlayer)
			{
				*peBestPlayer = (PlayerTypes)iBestPlayer;
			}
			return iBestValue;
		}

	}

	if (NULL != peBestPlayer)
	{
		*peBestPlayer = getID();
	}
	return iSpreadInternalValue;
}

//Returns approximately 100 x gpt value of the corporation.
int CvPlayerAI::AI_corporationValue(CorporationTypes eCorporation, const CvCity* pCity) const
{

	PROFILE_EXTRA_FUNC();
	if (pCity == NULL)
	{
		if (getCapitalCity() != NULL)
		{
			pCity = getCapitalCity();
		}
	}
	if (NULL == pCity)
	{
		return 0;
	}
	const CvCorporationInfo& kCorp = GC.getCorporationInfo(eCorporation);
	int iBonusValue = 0;

	for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); iBonus++)
	{
		const BonusTypes eBonus = (BonusTypes)iBonus;
		const int iBonusCount = pCity->getNumBonuses(eBonus);
		if (iBonusCount > 0)
		{
			foreach_(const BonusTypes ePrereqBonus, kCorp.getPrereqBonuses())
			{
				if (eBonus == ePrereqBonus)
				{
					//	These are all in hundredths, so the multipliers here accoutn for the division
					//	by 100 at the very end and are 100 times smaller than the multipliers for the
					//	absolute commerces/yields later
					//	Production is considerd wiorth 4 X gold, food 3 X
					iBonusValue += (300 * kCorp.getYieldProduced(YIELD_FOOD) * iBonusCount);
					iBonusValue += (400 * kCorp.getYieldProduced(YIELD_PRODUCTION) * iBonusCount);
					iBonusValue += (100 * kCorp.getYieldProduced(YIELD_COMMERCE) * iBonusCount);

					iBonusValue += (100 * kCorp.getCommerceProduced(COMMERCE_GOLD) * iBonusCount);
					iBonusValue += (100 * kCorp.getCommerceProduced(COMMERCE_RESEARCH) * iBonusCount);
					iBonusValue += (50 * kCorp.getCommerceProduced(COMMERCE_CULTURE) * iBonusCount);
					iBonusValue += (40 * kCorp.getCommerceProduced(COMMERCE_ESPIONAGE) * iBonusCount);

					if (NO_BONUS != kCorp.getBonusProduced())
					{
						int iBonuses = getNumAvailableBonuses((BonusTypes)kCorp.getBonusProduced());
						iBonusValue += (AI_baseBonusVal((BonusTypes)kCorp.getBonusProduced()) * 1000) / (1 + 3 * iBonuses * iBonuses);
					}
				}
			}
		}
	}
	iBonusValue *= 3;

	/************************************************************************************************/
	/* Afforess					  Start		 02/09/10											   */
	/*																							  */
	/*																							  */
	/************************************************************************************************/
	//TODO: Move this to CityAI?
	iBonusValue += kCorp.getHealth() * 15000;
	iBonusValue += kCorp.getHappiness() * 25000;
	iBonusValue += kCorp.getMilitaryProductionModifier() * 3500;
	iBonusValue += kCorp.getFreeXP() * 15000;

	//these are whole numbers, not like the percents above.
	iBonusValue += (30000 * kCorp.getYieldChange(YIELD_FOOD));
	iBonusValue += (40000 * kCorp.getYieldChange(YIELD_PRODUCTION));
	iBonusValue += (10000 * kCorp.getYieldChange(YIELD_COMMERCE));

	iBonusValue += (10000 * kCorp.getCommerceChange(COMMERCE_GOLD));
	iBonusValue += (10000 * kCorp.getCommerceChange(COMMERCE_RESEARCH));
	iBonusValue += (5000 * kCorp.getCommerceChange(COMMERCE_CULTURE));
	iBonusValue += (4000 * kCorp.getCommerceChange(COMMERCE_ESPIONAGE));
	/************************************************************************************************/
	/* Afforess						 END															*/
	/************************************************************************************************/

		//	Koshling - this result was 2-orders of magnitude out (relative to what the comment at
		//	the top of the routine claims).  The net result was that pretty much all corporation
		//	headquarters were in a totally different league from oher Wonders and actually triggered
		//	an integer overflow assertion failure in building assessment (for the headquarters)!
		//	Dividing by 100 to bring it back into line with what the header comment claims
	return iBonusValue / 100;
}

int CvPlayerAI::AI_areaMissionAIs(const CvArea* pArea, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			if (pLoopSelectionGroup->AI_getMissionAIType() == eMissionAI)
			{
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

				if (pMissionPlot != NULL)
				{
					if (pMissionPlot->area() == pArea)
					{
						iCount += pLoopSelectionGroup->getNumUnits();
					}
				}
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_plotTargetMissionAIsInternal(const CvPlot* pPlot, MissionAITypes eMissionAI, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

		if (eMissionAI == NO_MISSIONAI || eGroupMissionAI == eMissionAI)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= iRange)
				{
					iCount += pLoopSelectionGroup->getNumUnits();

					if (piClosest != NULL)
					{
						int iGroupDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());

						if (iGroupDistance < *piClosest)
						{
							*piClosest = iGroupDistance;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_plotTargetMissionAIsInternalinCargoVolume(const CvPlot* pPlot, MissionAITypes eMissionAI, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

		if (eMissionAI == NO_MISSIONAI || eGroupMissionAI == eMissionAI)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= iRange)
				{
					iCount += pLoopSelectionGroup->getNumUnitCargoVolumeTotal();

					if (piClosest != NULL)
					{
						const int iGroupDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());

						if (iGroupDistance < *piClosest)
						{
							*piClosest = iGroupDistance;
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_plotTargetMissionAIsinCargoVolume(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	//	Only cache 0-range, specific mission AI results
	MissionPlotTargetMap::const_iterator foundMissionPlotMapItr = ((iRange == 0 && eMissionAI != NO_MISSIONAI) ? m_missionTargetCache.find(eMissionAI) : m_missionTargetCache.end());

	if (foundMissionPlotMapItr == m_missionTargetCache.end())
	{
		if (iRange == 0 && eMissionAI != NO_MISSIONAI)
		{
			PlotMissionTargetMap& plotMissionMap = m_missionTargetCache[eMissionAI];

			//	Since we have to walk all the groups now anyway populate the full cache map for this missionAI
			foreach_(const CvSelectionGroup * group, groups())
			{
				const MissionAITypes eGroupMissionAI = group->AI_getMissionAIType();
				if (eGroupMissionAI != eMissionAI)
					continue;

				const CvPlot* pMissionPlot = group->AI_getMissionAIPlot();
				if (pMissionPlot == NULL)
					continue;

				const int iDistance = stepDistance(group->getX(), group->getY(), pMissionPlot->getX(), pMissionPlot->getY());
				{
					// Update cache
					MissionTargetInfo& info = plotMissionMap[pMissionPlot];
					info.iVolume += group->getNumUnitCargoVolumeTotal();
					info.iClosest = std::min(info.iClosest, iDistance);
				}

				if (pMissionPlot == pPlot)
				{
					iCount += group->getNumUnitCargoVolumeTotal();

					if (piClosest != NULL)
					{
						*piClosest = std::min(*piClosest, iDistance);
					}
				}
			}
		}
		else
		{
			iCount = AI_plotTargetMissionAIsInternalinCargoVolume(pPlot, eMissionAI, iRange, piClosest);
		}
	}
	else
	{
		const PlotMissionTargetMap& plotMissionTargetMap = foundMissionPlotMapItr->second;
		PlotMissionTargetMap::const_iterator foundPlotInfoItr = plotMissionTargetMap.find(pPlot);

		if (foundPlotInfoItr != plotMissionTargetMap.end())
		{
			const MissionTargetInfo& info = foundPlotInfoItr->second;

			iCount = info.iVolume;
			if (piClosest != NULL)
			{
				*piClosest = info.iClosest;
			}
		}
		else
		{
			iCount = 0;
		}
	}

	if (pSkipSelectionGroup != NULL &&
		 (pSkipSelectionGroup->AI_getMissionAIType() == eMissionAI || eMissionAI == NO_MISSIONAI) &&
		 pPlot == pSkipSelectionGroup->AI_getMissionAIPlot())
	{
		iCount -= pSkipSelectionGroup->getNumUnitCargoVolumeTotal();
	}

	return std::max(0, iCount);
}

int CvPlayerAI::AI_plotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup, int iRange, int* piClosest) const
{
	PROFILE_FUNC();

	int iCount = 0;
	if (piClosest != NULL)
	{
		*piClosest = MAX_INT;
	}

	//	Only cache 0-range, specific mission AI results
	MissionPlotTargetMap::const_iterator itr = ((iRange == 0 && eMissionAI != NO_MISSIONAI) ? m_missionTargetCache.find(eMissionAI) : m_missionTargetCache.end());

	if (itr == m_missionTargetCache.end())
	{
		if (iRange == 0 && eMissionAI != NO_MISSIONAI)
		{
			PlotMissionTargetMap& plotMissionTargetMap = m_missionTargetCache[eMissionAI];

			//	Since we have to walk all the groups now anyway populate the full cache map for this missionAI
			foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
			{
				const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

				if (eGroupMissionAI != eMissionAI)
					continue;
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();
				if (pMissionPlot == NULL)
					continue;

				const int iDistance = stepDistance(pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY(), pMissionPlot->getX(), pMissionPlot->getY());
				PlotMissionTargetMap::iterator plotMissionTargetItr = plotMissionTargetMap.find(pMissionPlot);

				if (plotMissionTargetItr != plotMissionTargetMap.end())
				{
					MissionTargetInfo& info = plotMissionTargetItr->second;
					info.iCount += pLoopSelectionGroup->getNumUnits();
					info.iClosest = std::min(info.iClosest, iDistance);
				}
				else
				{
					MissionTargetInfo info;

					info.iCount = 0;
					info.iClosest = iDistance;
					info.iCount = pLoopSelectionGroup->getNumUnits();

					plotMissionTargetMap.insert(std::make_pair(pMissionPlot, info));
				}

				if (pMissionPlot == pPlot)
				{
					iCount += pLoopSelectionGroup->getNumUnits();

					if (piClosest != NULL)
					{
						*piClosest = std::min(*piClosest, iDistance);
					}
				}
			}
		}
		else
		{
			iCount = AI_plotTargetMissionAIsInternal(pPlot, eMissionAI, iRange, piClosest);
		}
	}
	else
	{
		PlotMissionTargetMap::const_iterator plotMissionTargetItr = itr->second.find(pPlot);

		if (plotMissionTargetItr != itr->second.end())
		{
			iCount = plotMissionTargetItr->second.iCount;
			if (piClosest != NULL)
			{
				*piClosest = plotMissionTargetItr->second.iClosest;
			}
		}
		else
		{
			iCount = 0;
		}
	}

	if (pSkipSelectionGroup != NULL &&
		 (pSkipSelectionGroup->AI_getMissionAIType() == eMissionAI || eMissionAI == NO_MISSIONAI) &&
		 pPlot == pSkipSelectionGroup->AI_getMissionAIPlot())
	{
		iCount -= pSkipSelectionGroup->getNumUnits();
	}

	FASSERT_NOT_NEGATIVE(iCount);
	return iCount;
}

void CvPlayerAI::AI_noteMissionAITargetCountChange(MissionAITypes eMissionAI, const CvPlot* pPlot, int iChange, const CvPlot* pUnitPlot, int iVolume)
{
	MissionPlotTargetMap::iterator foundPlotMissionMapItr = m_missionTargetCache.find(eMissionAI);

	if (foundPlotMissionMapItr != m_missionTargetCache.end())
	{
		PlotMissionTargetMap& plotMissionTargetMap = foundPlotMissionMapItr->second;
		MissionTargetInfo& info = plotMissionTargetMap[pPlot];

		const int iDistance = (pUnitPlot == NULL ? MAX_INT : stepDistance(pPlot->getX(), pPlot->getY(), pUnitPlot->getX(), pUnitPlot->getY()));

		info.iCount += iChange;
		info.iVolume += iVolume;

		if (info.iCount == 0)
		{
			info.iClosest = MAX_INT;
		}
		else if (iDistance < info.iClosest)
		{
			info.iClosest = iDistance;
		}
	}
}


int CvPlayerAI::AI_cityTargetUnitsByPath(const CvCity* pCity, const CvSelectionGroup* pSkipSelectionGroup, int iMaxPathTurns) const
{
	PROFILE_FUNC();

	int iCount = 0;

	int iPathTurns;
	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup && pLoopSelectionGroup->plot() != NULL && pLoopSelectionGroup->getNumUnits() > 0)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot != NULL)
			{
				const int iDistance = stepDistance(pCity->getX(), pCity->getY(), pMissionPlot->getX(), pMissionPlot->getY());

				if (iDistance <= 1)
				{
					if (pLoopSelectionGroup->generatePath(pLoopSelectionGroup->plot(), pMissionPlot, 0, true, &iPathTurns))
					{
						if (!(pLoopSelectionGroup->canAllMove()))
						{
							iPathTurns++;
						}

						if (iPathTurns <= iMaxPathTurns)
						{
							iCount += pLoopSelectionGroup->getNumUnits();
						}
					}
				}
			}
		}
	}

	return iCount;
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_unitTargetMissionAIs(pUnit, &eMissionAI, 1, pSkipSelectionGroup, -1);
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_unitTargetMissionAIs(pUnit, aeMissionAI, iMissionAICount, pSkipSelectionGroup, -1, false);
}

int CvPlayerAI::AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup, int iMaxPathTurns, bool bCargo) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(CvSelectionGroup * group, groups())
	{
		if (group == pSkipSelectionGroup || group->AI_getMissionAIUnit() != pUnit)
		{
			continue;
		}
		int iPathTurns = MAX_INT;

		if (iMaxPathTurns >= 0 && pUnit->plot() != NULL && group->plot() != NULL)
		{
			// Determined the number of turns to reach the target unit
			group->generatePath(group->plot(), pUnit->plot(), 0, false, &iPathTurns);

			if (!group->canAllMove())
			{
				iPathTurns++;
			}
		}

		// If the mission parameters state any amount of movement is ok or the amount of movement allowed is valid
		if (iMaxPathTurns == -1 || iPathTurns <= iMaxPathTurns)
		{
			const MissionAITypes eGroupMissionAI = group->AI_getMissionAIType();

			for (int iMissionAIIndex = 0; iMissionAIIndex < iMissionAICount; iMissionAIIndex++)
			{
				//it looks like we're cycling through a predefined array (in one case we're matching to any one of
				//4 possible missions: MISSIONAI_LOAD_ASSAULT, MISSIONAI_LOAD_SETTLER, MISSIONAI_LOAD_SPECIAL, MISSIONAI_ATTACK_SPY
				if (eGroupMissionAI == aeMissionAI[iMissionAIIndex] || NO_MISSIONAI == aeMissionAI[iMissionAIIndex])
				{
					if (!bCargo)
					{
						iCount += group->getNumUnits();
					}
					else
					{
						iCount += group->getNumUnitCargoVolumeTotal();
					}
				}
			}
		}
	}
	return iCount;
}


int CvPlayerAI::AI_enemyTargetMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	return AI_enemyTargetMissionAIs(&eMissionAI, 1, pSkipSelectionGroup);
}

int CvPlayerAI::AI_enemyTargetMissionAIs(MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (NULL != pMissionPlot && pMissionPlot->isOwned())
			{
				const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();
				for (int iMissionAIIndex = 0; iMissionAIIndex < iMissionAICount; iMissionAIIndex++)
				{
					if (eGroupMissionAI == aeMissionAI[iMissionAIIndex] || NO_MISSIONAI == aeMissionAI[iMissionAIIndex])
					{
						if (GET_TEAM(getTeam()).AI_isChosenWar(pMissionPlot->getTeam()))
						{
							iCount += pLoopSelectionGroup->getNumUnits();
							iCount += pLoopSelectionGroup->getCargo();
							//Certainly intended to be a count
						}
					}
				}
			}
		}
	}

	return iCount;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  05/19/10								jdog5000	  */
/*																							  */
/* General AI																				   */
/************************************************************************************************/
int CvPlayerAI::AI_enemyTargetMissions(TeamTypes eTargetTeam, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	int iCount = 0;

	foreach_(const CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();

			if (pMissionPlot == NULL)
			{
				pMissionPlot = pLoopSelectionGroup->plot();
			}

			if (NULL != pMissionPlot)
			{
				if (pMissionPlot->isOwned() && pMissionPlot->getTeam() == eTargetTeam)
				{
					if (atWar(getTeam(), pMissionPlot->getTeam()) || pLoopSelectionGroup->AI_isDeclareWar(pMissionPlot))
					{
						iCount += pLoopSelectionGroup->getNumUnits();
						iCount += pLoopSelectionGroup->getCargo();
						//Certainly intended to be a count
					}
				}
			}
		}
	}

	return iCount;
}
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

int CvPlayerAI::AI_wakePlotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	int iCount = 0;

	foreach_(CvSelectionGroup * pLoopSelectionGroup, groups())
	{
		if (pLoopSelectionGroup != pSkipSelectionGroup)
		{
			const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();
			if (eMissionAI == NO_MISSIONAI || eMissionAI == eGroupMissionAI)
			{
				const CvPlot* pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot();
				if (pMissionPlot != NULL && pMissionPlot == pPlot)
				{
					iCount += pLoopSelectionGroup->getNumUnits();
					pLoopSelectionGroup->setActivityType(ACTIVITY_AWAKE);
				}
			}
		}
	}

	return iCount;
}


CivicTypes CvPlayerAI::AI_bestCivic(CivicOptionTypes eCivicOption) const
{
	int iBestValue;
	return AI_bestCivic(eCivicOption, &iBestValue, false);
}

CivicTypes CvPlayerAI::AI_bestCivic(CivicOptionTypes eCivicOption, int* iBestValue, bool bCivicOptionVacuum, CivicTypes* paeSelectedCivics) const
{
	PROFILE_EXTRA_FUNC();
	(*iBestValue) = MIN_INT;
	CivicTypes eBestCivic = NO_CIVIC;

	for (int iI = GC.getNumCivicInfos() - 1; iI > -1; iI--)
	{
		const CivicTypes eCivicX = static_cast<CivicTypes>(iI);

		if (GC.getCivicInfo(eCivicX).getCivicOptionType() == eCivicOption && canDoCivics(eCivicX))
		{
			const int iValue = AI_civicValue(eCivicX, bCivicOptionVacuum, paeSelectedCivics);

			if (iValue > (*iBestValue))
			{
				(*iBestValue) = iValue;
				eBestCivic = eCivicX;
			}
		}
	}
	FAssert(!bCivicOptionVacuum || eBestCivic != NO_CIVIC);
	return eBestCivic;
}


//	Provide a measure of overall happyness (weighted appropriately by city)
int CvPlayerAI::AI_getOverallHappyness(int iExtraUnhappy) const
{
	PROFILE_EXTRA_FUNC();
	int iHappyness = 0;

	foreach_(const CvCity * pLoopCity, cities() | filtered(CvCity::fn::isNoUnhappiness()))
	{
		iHappyness += (pLoopCity->happyLevel() - pLoopCity->unhappyLevel() - iExtraUnhappy) * 50;
	}

	return iHappyness;
}

//	Helper function to compute a trnuncated quadratic to asign a value to (net) happyness
static int happynessValue(int iNetHappyness)
{
	if (iNetHappyness > 0)
	{
		// Cap useful gains 0n the postive side
		if (iNetHappyness >= 5)
		{
			return 250; // Value from below calculation when iNetHappyness=5
		}
		return 100 * iNetHappyness - 10 * iNetHappyness * iNetHappyness;
	}
	return 100 * iNetHappyness; // Just linear on the negative side since each is one less working pop
}


int CvPlayerAI::AI_civicValue(CivicTypes eCivic, bool bCivicOptionVacuum, CivicTypes* paeSelectedCivics) const
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(-1, GC.getNumCivicInfos(), eCivic);

	if (eCivic == NO_CIVIC || isNPC())
	{
		return 1;
	}

	if (m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())] != MAX_INT)
	{
		return m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())];
	}
	const CvCivicInfo& kCivic = GC.getCivicInfo(eCivic);
	const CvTeamAI& pTeam = GET_TEAM(getTeam());

	bool bWarPlan = pTeam.hasWarPlan(true);
	if (bWarPlan)
	{
		bWarPlan = false;
		int iEnemyWarSuccess = 0;

		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
		{
			const CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
			if (kLoopTeam.isAlive() && !kLoopTeam.isMinorCiv() && pTeam.AI_getWarPlan((TeamTypes)iTeam) != NO_WARPLAN)
			{
				if (pTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_TOTAL || pTeam.AI_getWarPlan((TeamTypes)iTeam) == WARPLAN_PREPARING_TOTAL)
				{
					bWarPlan = true;
					break;
				}

				if (pTeam.AI_isLandTarget((TeamTypes)iTeam))
				{
					bWarPlan = true;
					break;
				}
				iEnemyWarSuccess += kLoopTeam.AI_getWarSuccess(getTeam());
			}
		}

		if (!bWarPlan)
		{
			if (iEnemyWarSuccess > std::min(getNumCities(), 4) * GC.getWAR_SUCCESS_CITY_CAPTURING() // Lots of fighting, so war is real
			|| iEnemyWarSuccess > std::min(getNumCities(), 2) * GC.getWAR_SUCCESS_CITY_CAPTURING()
			&& pTeam.AI_getEnemyPowerPercent() > 120)
			{
				bWarPlan = true;
			}
		}
	}

	ReligionTypes eBestReligion = AI_bestReligion();
	if (!kCivic.isStateReligion() && !isStateReligion())
	{
		eBestReligion = NO_RELIGION;
	}
	else if (eBestReligion == NO_RELIGION)
	{
		eBestReligion = getStateReligion();
	}

	//Fuyu Civic AI: restructuring
		//#0: constant values
	int iValue = getNumCities() * 6 + kCivic.getAIWeight() * getNumCities();

	if (gPlayerLogLevel > 2)
	{
		logBBAI("Civic %S base value %d", kCivic.getDescription(), iValue);
	}
	// Koshling - Anarchy length is not part of the civic's value - it's part of the cost of an overall switch and is now evaluated in that process
	//iValue += -(kCivic.getAnarchyLength() * getNumCities());

	int iTempValue = -getSingleCivicUpkeep(eCivic, true) * 80 / 100;
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S upkeep value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;

	CvCity* pCapital = getCapitalCity();
	iValue += ((kCivic.getGreatPeopleRateModifier() * getNumCities()) / 10);

	//	Koshling - made the GG calculations non-linear as they were not scaling well with large armies
	int iGGMultiplier = 100 - 1000 / (10 + range(getNumMilitaryUnits(), 1, 100));
	iTempValue = ((kCivic.getGreatGeneralRateModifier() * iGGMultiplier) / 10);
	iTempValue += ((kCivic.getDomesticGreatGeneralRateModifier() * iGGMultiplier) / 20);
	//Fuyu: Only if wars ongoing, as suggested by Munch - modified by Koshling to just be an increase then
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S GG modifier value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue / (bWarPlan || isMinorCiv() ? 3 : 1);
	iTempValue = -(kCivic.getDistanceMaintenanceModifier() * std::max(0, getNumCities() - 3) / 8);

	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S distance maintenance modifier value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;
	iTempValue = -(kCivic.getNumCitiesMaintenanceModifier() * std::max(0, getNumCities() - 3) / 8);

	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S #cities maintenance modifier value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;

	const int iWarmongerPercent = 25000 / std::max(100, 100 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarRand());

	if (kCivic.getFreeExperience() > 0)
	{
		// Free experience increases value of hammers spent on units, population is an okay measure of base hammer production
		iTempValue = (kCivic.getFreeExperience() * getTotalPopulation() * (bWarPlan ? 30 : 12)) / 100;
		iTempValue *= AI_averageYieldMultiplier(YIELD_PRODUCTION);
		iTempValue /= 100;
		iTempValue *= iWarmongerPercent;
		iTempValue /= 100;
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S free experience value %d", kCivic.getDescription(), iTempValue);
		}
		iValue += iTempValue;
	}

	iTempValue = ((kCivic.getWorkerSpeedModifier() * AI_getNumAIUnits(UNITAI_WORKER)) / 15);
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S worker speed value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;
	iTempValue = ((kCivic.getImprovementUpgradeRateModifier() * getNumCities()) / 50);
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S improvement upgrade modifier value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;
	iTempValue = (kCivic.getMilitaryProductionModifier() * getNumCities() * iWarmongerPercent) / (bWarPlan ? 300 : 500);
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S military production modifier value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;
	iTempValue = kCivic.getFreeUnitUpkeepCivilian() / 2;
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S free units value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;
	iTempValue = kCivic.getFreeUnitUpkeepMilitary() / 2;
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S free military units value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;
	iTempValue = kCivic.getFreeUnitUpkeepCivilianPopPercent() * getTotalPopulation() / 200;
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S 'free civilian unit upkeep per pop' value: %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;
	iTempValue = kCivic.getFreeUnitUpkeepMilitaryPopPercent() * getTotalPopulation() / 300;
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S 'free military unit upkeep per pop' value: %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;
	iTempValue = -(kCivic.getCivilianUnitUpkeepMod() * getNumUnits() - getNumMilitaryUnits());
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S civilian unit upkeep modifier %d%%",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = -kCivic.getMilitaryUnitUpkeepMod() * (bWarPlan ? (getNumMilitaryUnits() * 3 / 2) : getNumMilitaryUnits());

	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S military unit upkeep modifier %d%%",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	if (kCivic.getInflationModifier() != 0)
	{
		//	Koshling - Use 100 turns of first order costs to judge inflation modifiers
		iTempValue = static_cast<int>(-getInflationMod10000() * calculatePreInflatedCosts() * kCivic.getInflationModifier() / 10000);
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S inflation modifier value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	iTempValue = 0;
	if (kCivic.isMilitaryFoodProduction())
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			iTempValue += pLoopCity->foodDifference(false, true, true);
		}
		//If not at war Food is generally more valuable then hammers
		if (!bWarPlan)
		{
			iTempValue /= -4;
		}
		//If we are at war hammers are more valuable
		else
		{
			iTempValue *= 3;
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S military food production value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		// If there is no civicOption vacuum we need to subtract out the current civic
		CvCivicInfo* kCurrentCivic = NULL;

		if (!bCivicOptionVacuum)
		{
			CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOptionType());

			if (eCurrentCivic != NO_CIVIC)
			{
				kCurrentCivic = &GC.getCivicInfo(eCurrentCivic);
			}
		}
		if (kCivic.getRevIdxLocal() != 0)
		{
			//	What's our current situation?
			int	localRevIdx = AI_calculateAverageLocalInstability();

			//	Use the more serious of the before and after values if this civic were to be chosen
			if (kCivic.getRevIdxLocal() > 0)
			{
				localRevIdx += kCivic.getRevIdxLocal();
			}

			//	If there is no civoption vacuum we need to subtract out the current civic
			if (kCurrentCivic != NULL)
			{
				localRevIdx -= kCurrentCivic->getRevIdxLocal();
			}

			//	Treat instability seriously as it goes up - not just linear
			const int localRevScaling = localRevIdx < 0 ? 0 : std::min(localRevIdx * localRevIdx / 50 + localRevIdx / 2, 100);

			iTempValue = -(kCivic.getRevIdxLocal() * localRevScaling * getNumCities()) / 4;
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S local stability value %d", kCivic.getDescription(), iTempValue);
			}
			iValue += iTempValue;
		}

		if (kCivic.getRevIdxNational() != 0)
		{
			iTempValue = -(8 * getNumCities()) * kCivic.getRevIdxNational();

			//	If there is no civoption vacuum we need to subtract out the current civic
			if (kCurrentCivic != NULL)
			{
				iTempValue += (8 * getNumCities()) * kCurrentCivic->getRevIdxNational();
			}
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S national stability value %d", kCivic.getDescription(), iTempValue);
			}
			iValue += iTempValue;
		}

		if (kCivic.getRevIdxDistanceModifier() != 0)
		{
			int iCapitalDistance = AI_calculateAverageCityDistance();
			int iOldCapitalDistance = iCapitalDistance;
			iCapitalDistance *= 100 + kCivic.getRevIdxDistanceModifier() - (kCurrentCivic == NULL ? 0 : kCurrentCivic->getRevIdxDistanceModifier());
			iCapitalDistance /= 100;

			iTempValue = (getNumCities() * (iOldCapitalDistance - iCapitalDistance) * (10 + std::max(0, AI_calculateAverageLocalInstability())));
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S REV distance modifier value %d",
						 kCivic.getDescription(),
						 iTempValue);
			}
			iValue += iTempValue;
		}
	}

	if (kCivic.getCityLimit(getID()) > 0)
	{
		if (kCivic.getCityOverLimitUnhappy() == 0)
		{
			//	Treat numCities == limit as a want-to-expand case even if we have not actually (yet) decided
			//	to produce the settler
			if (getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) >= kCivic.getCityLimit(getID()))
			{
				iValue -= (getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) + 1 - kCivic.getCityLimit(getID())) * 100; //if we are planning to expand, city limitations suck
			}
			else
			{
				//	Smaller limits suck more but since we are not trying to expand it can't be
				//	worse than the best case where we ARE trying and can't
				iValue -= 100 / kCivic.getCityLimit(getID());
			}
		}
		else
		{
			//	Happiness effect calculation
			int iCost = 0;
			int iCount = 0;
			int iWantToBuild = getNumCities() + AI_totalUnitAIs(UNITAI_SETTLE) + 1;

			if (iWantToBuild > kCivic.getCityLimit(getID()))
			{
				int iExtraCities = iWantToBuild - kCivic.getCityLimit(getID());

				foreach_(const CvCity * pLoopCity, cities() | filtered(!CvCity::fn::isNoUnhappiness()))
				{
					const int iHappy = pLoopCity->happyLevel() - pLoopCity->unhappyLevel(3);	//	Allow for pop growth of 3

					iCount++;

					if (iHappy < iExtraCities * kCivic.getCityOverLimitUnhappy())
					{
						//	Weight by city size as the happiness calculation does
						//	[TBD - is this really right though - unhappy in smaller cities is arguably worse]
						iCost += 50 * (iExtraCities * kCivic.getCityOverLimitUnhappy() - iHappy);
					}
				}

				//	Same normalization as the happiness calculations later use
				if (iCount != 0)
				{
					int iTempValue = (getNumCities() * 3 * iCost) / (25 * iCount);

					iValue -= iTempValue;

					if (gPlayerLogLevel > 2)
					{
						logBBAI("Civic %S city limit bad value %d",
								 kCivic.getDescription(),
								 iTempValue);
					}
				}
			}
		}
	}

	//Upgrade Anywhere
	iTempValue = 0;
	if (kCivic.isUpgradeAnywhere())
	{
		iTempValue += getNumMilitaryUnits() * iWarmongerPercent / 100;

		if (bWarPlan)
		{
			iTempValue *= 2;
			//the current gold we have plus the gold we will have in 10 turns is a decent
			//estimate of whether we are rich (if we can afford to upgrade units, anyway)
			if (getGold() + 10 * calculateBaseNetGold() > 50 + 100 * getCurrentEra())
			{
				iTempValue *= 2;
			}
		}
		else
		{
			iTempValue /= 2;
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S upgrade anywhere value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}
	bool bValid = true;
	iTempValue = 0;
	int iNonstateReligionCount = 0;
	//Inquisition Civic Values
	if (kCivic.isAllowInquisitions())
	{
		if (getStateReligion() != NO_RELIGION)
		{
			//check that we don't have a civic that already blocks this inquisitions...
			for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
			{
				//we are considering changing this civic, so ignore it
				if (kCivic.getCivicOptionType() != (CivicOptionTypes)iI)
				{
					if (GC.getCivicInfo(getCivics((CivicOptionTypes)iI)).isDisallowInquisitions())
					{
						bValid = false;
					}
				}
			}
			if (bValid && hasInquisitionTarget())
			{
				for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
				{
					if ((ReligionTypes)iI != getStateReligion())
					{
						iNonstateReligionCount += algo::count_if(cities(), CvCity::fn::isHasReligion((ReligionTypes)iI));
					}
				}
			}
		}
		if (isPushReligiousVictory())
		{
			iTempValue += iNonstateReligionCount * 20;
		}
		else if (isConsiderReligiousVictory())
		{
			iTempValue /= 5;
		}
		iTempValue += countCityReligionRevolts() * 5;
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S inquisitions value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		iTempValue += (kCivic.getUnitCombatProductionModifier(iI) * 2) / 3;
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S unitcombat production modifier value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	foreach_(const BuildingModifier2 & modifier, kCivic.getBuildingProductionModifiers())
	{
		iTempValue += (modifier.second * 2) / 5;
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S building production modifier value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		iTempValue += kCivic.getBonusMintedPercent(iI) * getNumAvailableBonuses((BonusTypes)iI) * getNumCities() * 2;
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S bonus minting value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		iTempValue += (kCivic.getUnitProductionModifier(iI) * 2) / 5;
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S unit class production modifier value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	if (kCivic.getPopulationgrowthratepercentage() != 0)
	{
		if (m_iCityGrowthValueBase == -1)
		{
			iTempValue = 0;

			foreach_(const CvCity * pLoopCity, cities())
			{
				int iCityHappy = pLoopCity->happyLevel() - pLoopCity->unhappyLevel();
				int iCurrentFoodToGrow = pLoopCity->growthThreshold();
				int iFoodPerTurn = pLoopCity->foodDifference(false, true, true);
				int iCityValue = 0;

				if (iFoodPerTurn > 0 && iCityHappy >= 0)
				{
					iCityValue += (std::min(3, iCityHappy + 1) * iCurrentFoodToGrow) / iFoodPerTurn;
				}

				iTempValue += iCityValue;
			}
		}
		else
		{
			iTempValue = m_iCityGrowthValueBase;
		}

		if (getNumCities() > 0)
		{
			//	iTempValue is now essentially the average number of turns to grow * iCityCount
			//	We want to normalize the value to be somewhere near gold-per-turn units, so that's
			//	roughly the amount of extra 'value' an extra population has multiplied by the
			//	reduction in turns to grow.  The value of an extra pop is more or less the yield
			//	value it produces which can be estimated as a constant that varies with era, reflecting
			//	increased producitivity of farms, mines, specialists, etc.  (era-index * 2 + 3) is a reasonable
			//	estimate.
			//	So the value is:
			int onePopBaseValue = (int)getCurrentEra() * 2 + 3;
			iTempValue = -(kCivic.getPopulationgrowthratepercentage() * iTempValue * onePopBaseValue) / (getNumCities() * 100);

			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S growth rate modifier value %d",
						 kCivic.getDescription(),
						 iTempValue);
			}
			iValue += iTempValue;
		}
	}

	if (kCivic.getAttitudeShareMod() != 0)
	{
		int iTempValue = 0;

		// The AI will disfavor bad attitude modifiers more than good ones
		if (kCivic.getAttitudeShareMod() < 0)
		{
			iTempValue += (kCivic.getAttitudeShareMod() * 4);
		}
		else if (kCivic.getAttitudeShareMod() > 0)
		{
			iTempValue += (kCivic.getAttitudeShareMod() * 3);
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S attitude share value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	if (bWarPlan)
	{
		int iTempValue = 0;

		if (kCivic.getFreedomFighterChange() > 0)
		{
			iTempValue += (kCivic.getFreedomFighterChange() * 10);
		}
		if (kCivic.isFreedomFighter())
		{
			iTempValue += 25;
		}
		//negative values are good, positive ones, bad
		if (kCivic.getDistantUnitSupportCostModifier() < 0)
		{
			iTempValue = (-kCivic.getDistantUnitSupportCostModifier() * 2);
		}
		//if we are going to war soon, we can't afford high costs
		else if (kCivic.getDistantUnitSupportCostModifier() > 0)
		{
			iTempValue = (-kCivic.getDistantUnitSupportCostModifier() * 3);
		}
		//City defense is good, especially during wars
		if (kCivic.getExtraCityDefense() > 0)
		{
			iTempValue += (kCivic.getExtraCityDefense() * 2);
		}
		//Negative city defense would be really bad in a war, avoid at all costs
		else if (kCivic.getExtraCityDefense() < 0)
		{
			iTempValue -= (kCivic.getExtraCityDefense() * 4);
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S war-time modifier value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}
	else
	{
		iTempValue = (-kCivic.getDistantUnitSupportCostModifier() / 2);
		iTempValue += kCivic.getExtraCityDefense();
		if (kCivic.getFreedomFighterChange() > 0)
		{
			iTempValue += (kCivic.getFreedomFighterChange() * 2);
		}
		if (kCivic.isFreedomFighter())
		{
			iTempValue += 5;
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S non-war-time modifier value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	if (kCivic.getTaxRateUnhappiness() != 0)
	{
		int iNewAnger = (getCommercePercent(COMMERCE_GOLD) * getTaxRateUnhappiness() / 100);

		iTempValue = (12 * getNumCities() * AI_getHappinessWeight(-iNewAnger, 0)) / 100;
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S tax rate unhappiness value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		if (kCivic.getFreeSpecialistCount(iI) > 0)
		{
			iTempValue += getNumCities() * kCivic.getFreeSpecialistCount(iI) * 12;
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S free speciailist value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;

	if (pCapital != NULL)
	{
		//	Warm up the can train cache for the capital
		pCapital->populateCanTrainCache(false);

		const CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOptionType());

		for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
		{
			const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iI);
			bool bValidCivicsWith = true;
			bool bValidCivicsWithout = true;
			bool bCivicIsEnabler = false;

			if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() == NO_TECH || GC.getTechInfo((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()).getEra() <= getCurrentEra())
			{
				if ((GC.getBuildingInfo(eLoopBuilding).isPrereqAndCivics(eCivic) || GC.getBuildingInfo(eLoopBuilding).isPrereqOrCivics(eCivic)) ||
					(eCurrentCivic != NO_CIVIC && (GC.getBuildingInfo(eLoopBuilding).isPrereqAndCivics(eCurrentCivic) || GC.getBuildingInfo(eLoopBuilding).isPrereqOrCivics(eCurrentCivic))))
				{
					if (!GC.getGame().isBuildingMaxedOut(eLoopBuilding) || getBuildingCount((BuildingTypes)iI) > 0)
					{
						bool bValidWith = false;
						bool bValidWithout = false;
						bool bHasMultipleEnablingCivicCategories = false;
						CivicOptionTypes eEnablingCategory = NO_CIVICOPTION;

						for (int iJ = 0; iJ < GC.getNumCivicInfos(); iJ++)
						{
							if (GC.getBuildingInfo(eLoopBuilding).isPrereqAndCivics(iJ))
							{
								if (eEnablingCategory == NO_CIVICOPTION)
								{
									eEnablingCategory = (CivicOptionTypes)GC.getCivicInfo((CivicTypes)iJ).getCivicOptionType();
								}
								else
								{
									bHasMultipleEnablingCivicCategories = true;
								}

								if (eCivic != iJ)
								{
									if (kCivic.getCivicOptionType() == GC.getCivicInfo((CivicTypes)iJ).getCivicOptionType())
									{
										bValidCivicsWith = false;
										bValidCivicsWithout = (!bCivicOptionVacuum && eCurrentCivic == iJ);
									}
									else
									{
										if (!isCivic((CivicTypes)iJ))
										{
											bValidCivicsWith = false;
											bValidCivicsWithout = false;
										}
									}
								}
								else
								{
									bCivicIsEnabler = true;
									bValidCivicsWithout = false;
								}
							}
						}
						//Make sure we have the correct prereq Or Civics. We need just one
						if (bValidCivicsWith || bValidCivicsWithout)
						{
							bool bHasOrCivicReq = false;

							for (int iJ = 0; iJ < GC.getNumCivicInfos(); iJ++)
							{
								if (GC.getBuildingInfo(eLoopBuilding).isPrereqOrCivics(iJ))
								{
									if (eEnablingCategory == NO_CIVICOPTION)
									{
										eEnablingCategory = (CivicOptionTypes)GC.getCivicInfo((CivicTypes)iJ).getCivicOptionType();
									}
									else
									{
										bHasMultipleEnablingCivicCategories = true;
									}

									bHasOrCivicReq = true;

									if (kCivic.getCivicOptionType() != GC.getCivicInfo((CivicTypes)iJ).getCivicOptionType())
									{
										if (isCivic((CivicTypes)iJ))
										{
											bValidWithout = true;
											bValidWith = true;
										}
									}
									else if (eCivic == (CivicTypes)iJ)
									{
										bValidWith = true;
										bCivicIsEnabler = true;
									}
									else
									{
										bValidWithout |= (!bCivicOptionVacuum && eCurrentCivic == iJ);
									}
								}
							}

							if (bHasOrCivicReq)
							{
								bValidCivicsWith &= bValidWith;
								bValidCivicsWithout &= bValidWithout;
							}
						}

						iTempValue = 0;
						int iValueDivisor = 1;

						if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() != NO_TECH &&
							!pTeam.isHasTech((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()))
						{
							iValueDivisor = 2;
						}

						const int iNumInstancesToScore = isLimitedWonder(eLoopBuilding) ? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4);

						//	If the building is enabled by multiple categories just count it at half value always
						//	This isn't strictly accurate, but because civic evaluation works by linarly combining
						//	evaluations in different categories, cross-category couplings like this have to give
						//	stable result or else civic choices will oscillate
						if (bHasMultipleEnablingCivicCategories && bValidCivicsWith && bCivicIsEnabler)
						{
							//Estimate value from capital city
							iTempValue = (pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore) / (12 * iValueDivisor);
							if (gPlayerLogLevel > 2)
							{
								logBBAI("Civic %S jointly enables building %S with value %d",
									kCivic.getDescription(),
									GC.getBuildingInfo(eLoopBuilding).getDescription(),
									iTempValue);
							}
						}
						else if (bValidCivicsWith && !bValidCivicsWithout)
						{
							//Estimate value from capital city
							iTempValue = (pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore) / (6 * iValueDivisor);
							if (gPlayerLogLevel > 2)
							{
								logBBAI("Civic %S enables building %S with value %d",
									kCivic.getDescription(),
									GC.getBuildingInfo(eLoopBuilding).getDescription(),
									iTempValue);
							}
						}
						else if (!bValidCivicsWith && bValidCivicsWithout)
						{
							//Loses us the ability to construct the building
							iTempValue = -(pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore) / (6 * iValueDivisor);
							if (gPlayerLogLevel > 2)
							{
								logBBAI("Civic %S disables building %S with value %d",
									kCivic.getDescription(),
									GC.getBuildingInfo(eLoopBuilding).getDescription(),
									iTempValue);
							}
						}
						iValue += iTempValue;
					}
				}
			}
		}

		//TB Note: I hope I did this ok... I broke it down a little since the bools used in the building example were not termed to provide very strong clarity enough to 'get' what they were accomplishing
		//What I fear I may need to give greater consideration to here is the CATEGORY of civics and somehow include some indication of whether a trait is providing this or not.
		//All Religions Active
		if (GC.getGame().isOption(GAMEOPTION_RELIGION_DISABLING))
		{
			iTempValue = 0;
			if (kCivic.isAllReligionsActive())
			{
				ReligionTypes eCurrentReligion = getStateReligion();
				bool bHasEnablingCivic = (hasAllReligionsActive());
				bool bHasMultipleEnablingCivicCategories = (getAllReligionsActiveCount() > 1);

				for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
				{
					const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iI);

					if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() == NO_TECH || GC.getTechInfo((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()).getEra() <= getCurrentEra())
					{
						if (getBuildingCount(eLoopBuilding) > 0)
						{
							if (GC.getBuildingInfo(eLoopBuilding).getReligionType() != eCurrentReligion)
							{
								int iValueDivisor = 1;

								if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() != NO_TECH &&
									!pTeam.isHasTech((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()))
								{
									iValueDivisor = 2;
								}

								const int iNumInstancesToScore = isLimitedWonder(eLoopBuilding) ? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4);

								//	If the building is enabled by multiple categories just count it at half value always
								//	This isn't strictly accurate, but because civic evaluation works by linarly combining
								//	evaluations in different categories, cross-category couplings like this have to give
								//	stable result or else civic choices will oscillate
								if (bHasMultipleEnablingCivicCategories || bHasEnablingCivic)
								{
									//Estimate value from capital city
									iTempValue = pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore / (12 * iValueDivisor);
									if (gPlayerLogLevel > 2)
									{
										logBBAI("Civic %S jointly religiously enables building %S with value %d",
											kCivic.getDescription(),
											GC.getBuildingInfo(eLoopBuilding).getDescription(),
											iTempValue);
									}
								}
								else
								{
									//Estimate value from capital city
									iTempValue = pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore / (6 * iValueDivisor);
									if (gPlayerLogLevel > 2)
									{
										logBBAI("Civic %S religiously enables building %S with value %d",
											kCivic.getDescription(),
											GC.getBuildingInfo(eLoopBuilding).getDescription(),
											iTempValue);
									}
								}
							}
						}
					}
				}
			}

			iTempValue = 0;
			if (!kCivic.isAllReligionsActive())
			{
				const ReligionTypes eCurrentReligion = getStateReligion();
				const bool bHasEnablingCivic = hasAllReligionsActive();
				const bool bHasMultipleEnablingCivicCategories = getAllReligionsActiveCount() > 1;

				if (bHasEnablingCivic || bHasMultipleEnablingCivicCategories)
				{
					for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
					{
						const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iI);

						if (getBuildingCount(eLoopBuilding) > 0
						&&
						(
							GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() == NO_TECH
							||
							GC.getTechInfo((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()).getEra() <= getCurrentEra()
							)
						&& GC.getBuildingInfo(eLoopBuilding).getReligionType() != eCurrentReligion)
						{
							//Loses us the ability to construct the building
							int iValueDivisor = bHasMultipleEnablingCivicCategories ? 12 : 6;

							if (GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech() != NO_TECH
							&& !pTeam.isHasTech((TechTypes)GC.getBuildingInfo(eLoopBuilding).getPrereqAndTech()))
							{
								iValueDivisor *= 2;
							}
							const int iNumInstancesToScore = (isLimitedWonder(eLoopBuilding) ? 1 : std::max(getNumCities(), getBuildingCount(eLoopBuilding) + getNumCities() / 4));

							iTempValue = pCapital->AI_buildingValue(eLoopBuilding, 0) * iNumInstancesToScore / iValueDivisor;

							if (gPlayerLogLevel > 2)
							{
								logBBAI("Civic %S religiously disables building %S with value %d",
									kCivic.getDescription(),
									GC.getBuildingInfo(eLoopBuilding).getDescription(),
									iTempValue);
							}
							iValue -= iTempValue;
						}
					}
				}
			}
		}
	}

	bool bFinancialTrouble = AI_isFinancialTrouble();
	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iI);

		for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
		{
			if (kCivic.getBuildingCommerceModifier(eLoopBuilding, iJ) != 0)
			{
				const int iMulitplier = (iJ == COMMERCE_GOLD && bFinancialTrouble) ? 6 : 4;
				iTempValue += getBuildingCount((BuildingTypes)iI) * kCivic.getBuildingCommerceModifier(eLoopBuilding, iJ) * iMulitplier;
			}
			if (kCivic.getBuildingCommerceChange(iI, iJ) != 0)
			{
				const int iMulitplier = (iJ == COMMERCE_GOLD && bFinancialTrouble) ? 12 : 8;
				iTempValue += getBuildingCount((BuildingTypes)iI) * kCivic.getBuildingCommerceChange(iI, iJ) * iMulitplier;
			}
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S building commerce value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iJ = 0; iJ < GC.getNumImprovementInfos(); iJ++)
	{
		iTempValue += (8 * (kCivic.getImprovementHappinessChanges(iJ) * (getImprovementCount((ImprovementTypes)iJ) + getNumCities())));
		iTempValue += ((8 * (kCivic.getImprovementHealthPercentChanges(iJ) * (getImprovementCount((ImprovementTypes)iJ) + getNumCities()))) / 100);
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S improvement change value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
		{
			iTempValue += ((kCivic.getSpecialistCommercePercentChanges(iI, iJ) * getTotalPopulation()) / 500);
		}
		for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
		{
			iTempValue += ((kCivic.getSpecialistYieldPercentChanges(iI, iJ) * getTotalPopulation()) / 500);
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S specialist change value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		for (int iJ = 0; iJ < GC.getNumTerrainInfos(); iJ++)
		{
			iTempValue += (AI_averageYieldMultiplier((YieldTypes)iI) * (kCivic.getTerrainYieldChanges(iJ, iI) * (NUM_CITY_PLOTS + getNumCities() / 2))) / 100;
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S yield change value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumFlavorTypes(); iI++)
	{
		iValue += AI_getFlavorValue((FlavorTypes)iI) * kCivic.getFlavorValue((FlavorTypes)iI);
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S flavor value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = 0;

	CivicTypes eTargetCivic;
	CivicTypes eCurrentCivic = getCivics((CivicOptionTypes)kCivic.getCivicOptionType());
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		int iOurPower = std::max(1, pTeam.getPower(true));
		int iTheirPower = std::max(1, GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getDefensivePower());

		if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && GET_PLAYER((PlayerTypes)iI).isAlive() && GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
		{
			int iPlayerValue = 0;
			for (int iJ = 0; iJ < GC.getNumCivicOptionInfos(); iJ++)
			{
				eTargetCivic = GET_PLAYER((PlayerTypes)iI).getCivics((CivicOptionTypes)iJ);
				int iAttitudeChange = (eTargetCivic != NO_CIVIC ? (kCivic.getCivicAttitudeChange(eTargetCivic) - (eCurrentCivic != NO_CIVIC ? GC.getCivicInfo(eCurrentCivic).getCivicAttitudeChange(eTargetCivic) : 0)) : 0);
				//New Civic Attitude minus old civic attitude
				int iCurrentAttitude = AI_getAttitudeVal((PlayerTypes)iI);
				//We are close friends
				if (iCurrentAttitude > 5)
				{//Positive Changes are welcome, negative ones, not so much
					iPlayerValue += iAttitudeChange * 3;
				}
				//we aren't friends
				else
				{//if we aren't gearing up for a war yet...
					if (pTeam.AI_getWarPlan((TeamTypes)iI) != NO_WARPLAN)
					{//Then we would welcome some diplomatic improvements
						iPlayerValue += iAttitudeChange * 3;
						iPlayerValue /= 2;
					}
					else
					{
						//We are going to war, screw diplomacy
					}
				}
			}
			if (pTeam.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
			{//Who cares about vassals?
				iPlayerValue /= 5;
			}
			float fPowerRatio = ((float)iTheirPower) / ((float)iOurPower);
			iTempValue += (int)((float)iPlayerValue * fPowerRatio);
		}
	}
	if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
	{
		iTempValue /= 10;
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S attitude value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	iTempValue = (AI_RevCalcCivicRelEffect(eCivic));
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S REV effect value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	if (getWorldSizeMaxConscript(eCivic) > 0 && (pCapital != NULL))
	{
		UnitTypes eConscript = pCapital->getConscriptUnit();
		if (eConscript != NO_UNIT)
		{
			// Nationhood
			int iCombatValue = GC.getGame().AI_combatValue(eConscript);
			if (iCombatValue > 33)
			{
				iTempValue = getNumCities() + ((bWarPlan) ? 30 : 10);

				iTempValue *= range(pTeam.AI_getEnemyPowerPercent(), 50, 300);
				iTempValue /= 100;

				iTempValue *= iCombatValue;
				iTempValue /= 75;

				int iWarSuccessRatio = pTeam.AI_getWarSuccessCapitulationRatio();
				if (iWarSuccessRatio < -25)
				{
					iTempValue *= 75 + range(-iWarSuccessRatio, 25, 100);
					iTempValue /= 100;
				}

				if (gPlayerLogLevel > 2 && iTempValue != 0)
				{
					logBBAI("Civic %S conscription value %d",
							 kCivic.getDescription(),
							 iTempValue);
				}
				iValue += iTempValue;
			}
		}
	}
	if (bWarPlan)
	{
		iTempValue = ((kCivic.getExpInBorderModifier() * getNumMilitaryUnits()) / 200);
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S exp in borders value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}
	iTempValue = -((kCivic.getWarWearinessModifier() * getNumCities()) / ((bWarPlan) ? 10 : 50));
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S war weariness value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;
	iTempValue = (kCivic.getFreeSpecialist() * getNumCities() * 12);
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S free specialist value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	int iYieldValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		iTempValue = 0;

		iTempValue += ((kCivic.getYieldModifier(iI) * getNumCities()) / 2);

		if (pCapital)
		{
			// Bureaucracy
			// Benefit of having a supercity is higher than just increases in yield since will also win more
			// wonder races, build things that much faster
			//iTempValue += ((kCivic.getCapitalYieldModifier(iI)) / 2);

			//iTemp *= pCapital->AI_yieldMultiplier((YieldTypes)iI);
			//iTemp /= 100;
			iTempValue += (kCivic.getCapitalYieldModifier(iI) * pCapital->getPlotYield((YieldTypes)iI)) / 80;
		}
		iTempValue += ((kCivic.getTradeYieldModifier(iI) * getNumCities()) / 11);

		for (int iJ = 0; iJ < GC.getNumImprovementInfos(); iJ++)
		{
			// Free Speech
			iTempValue += (AI_averageYieldMultiplier((YieldTypes)iI) * (kCivic.getImprovementYieldChanges(iJ, iI) * (getImprovementCount((ImprovementTypes)iJ) + getNumCities() / 2))) / 100;
		}

		if (iI == YIELD_FOOD)
		{
			iTempValue *= 3;
		}
		else if (iI == YIELD_PRODUCTION)
		{
			iTempValue *= ((AI_avoidScience()) ? 6 : 2);
		}
		else if (iI == YIELD_COMMERCE)
		{
			iTempValue *= ((AI_avoidScience()) ? 2 : 4);
			iTempValue /= 3;
		}

		iYieldValue += iTempValue;
	}
	if (gPlayerLogLevel > 2 && iYieldValue != 0)
	{
		logBBAI("Civic %S yield value %d",
				 kCivic.getDescription(),
				 iYieldValue);
	}
	iValue += iYieldValue;

	const bool bCultureVictory2 = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2);
	const bool bCultureVictory3 = AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3);

	int iCommerceValue = 0;
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		iTempValue = 0;

		// Nationhood
		iTempValue += ((kCivic.getCommerceModifier(iI) * getNumCities()) / 3);
		iTempValue += (kCivic.getCapitalCommerceModifier(iI) / 2);
		if (iI == COMMERCE_ESPIONAGE)
		{
			iTempValue *= AI_getEspionageWeight();
			iTempValue /= 500;
		}

		// Representation
		iTempValue += kCivic.getSpecialistExtraCommerce(iI) * getTotalPopulation() / 15;

		iTempValue *= AI_commerceWeight((CommerceTypes)iI);

		if ((iI == COMMERCE_CULTURE) && bCultureVictory2)
		{
			iTempValue *= 2;
			if (bCultureVictory3)
			{
				iTempValue *= 2;
			}
		}
		iTempValue /= 100;

		iCommerceValue += iTempValue;
	}
	if (gPlayerLogLevel > 2 && iCommerceValue != 0)
	{
		logBBAI("Civic %S commerce value %d",
				 kCivic.getDescription(),
				 iCommerceValue);
	}
	iValue += iCommerceValue;

	//Everything hereafter requires at least civic option vacuum

	CivicTypes eCivicOptionCivic = getCivics((CivicOptionTypes)(kCivic.getCivicOptionType()));


	//#1: Happiness
	if (getNumCities() > 0
	&& (kCivic.getCivicPercentAnger() != 0 || kCivic.getCivicHappiness() != 0
		|| kCivic.getHappyPerMilitaryUnit() != 0 || kCivic.getLargestCityHappiness() != 0
		|| kCivic.isAnyBuildingHappinessChange() || kCivic.isAnyFeatureHappinessChange()
		|| kCivic.getNonStateReligionHappiness() != 0
		|| (kCivic.getWarWearinessModifier() != 0 && getWarWearinessPercentAnger() != 0)
		|| (kCivic.getCityLimit(getID()) > 0 && kCivic.getCityOverLimitUnhappy() > 0)
		|| (kCivic.getStateReligionHappiness() != 0 && (kCivic.isStateReligion() || isStateReligion()))))
	{
		int iExtraPop = 1;
		int iCount = 0;

		int iHappyValue = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityHappy = pLoopCity->happyLevel() - pLoopCity->unhappyLevel(iExtraPop);

			iCityHappy -= std::max(0, pLoopCity->getCommerceHappiness());

			int iMilitaryHappinessDefenders = 0;
			if (getHappyPerMilitaryUnit() != 0 || kCivic.getHappyPerMilitaryUnit() != 0)
			{
				//only count happiness from units that are expected to stay inside the city. Maximum 3
				iMilitaryHappinessDefenders = std::max(0, (pLoopCity->plot()->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID(), NO_TEAM, PUF_isCityAIType)
					- pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, NULL, getID()) - ((pLoopCity->getProductionUnitAI() == UNITAI_SETTLE) ? 1 : 0)));
				if (iMilitaryHappinessDefenders >= 4)
					iMilitaryHappinessDefenders = 3;
				//else
				//	iMilitaryHappinessDefenders = std::min(2, iMilitaryHappinessDefenders);
				if (getHappyPerMilitaryUnit() != 0)
				{
					iCityHappy -= pLoopCity->getMilitaryHappiness();
					//iCityHappy += getHappyPerMilitaryUnit() * iMilitaryHappinessDefenders;
				}
			}
			//eBestReligion may not be state religion but is treated as if it was
			if (eBestReligion != NO_RELIGION && isStateReligion() && getStateReligion() != eBestReligion)
			{
				if (getStateReligion() != NO_RELIGION && pLoopCity->isHasReligion(getStateReligion()))
				{
					iCityHappy -= getStateReligionHappiness();
					iCityHappy += getNonStateReligionHappiness();
				}
				if (pLoopCity->isHasReligion(eBestReligion))
				{
					iCityHappy -= getNonStateReligionHappiness();
					iCityHappy += getStateReligionHappiness();
				}
			}

			if (!bCivicOptionVacuum)
			{
				//int iCivicOptionHappy;
				iCityHappy -= pLoopCity->getAdditionalHappinessByCivic(eCivicOptionCivic, false, bCivicOptionVacuum, eBestReligion, iExtraPop, 0);
			}

			//Happy calculation
			int iHappy = pLoopCity->getAdditionalHappinessByCivic(eCivic, false, bCivicOptionVacuum, eBestReligion, iExtraPop, std::max(0, iMilitaryHappinessDefenders));

			int iHappyNow = iCityHappy;
			int iHappyThen = iCityHappy + iHappy;

			//	Factored original manipulation of the values into a sub-routine, and
			//	modified (in that sub-routine) to better handle negative values
			iTempValue = happynessValue(iHappyThen) - happynessValue(iHappyNow);

			iHappyValue += iTempValue * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);

			//iCount++;
			iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
			//if (iCount > 6)
			//{
			//	break;
			//}
		}

		//return (0 == iCount) ? 50 * iHappy : iHappyValue / iCount;

		if (iCount <= 0)
		{
			//iValue += (getNumCities() * 12 * 50*iHappy) / 100; //always 0 because getNumCities() is 0
		}
		else
		{
			//iValue += (getNumCities() * 12 * iHappyValue) / (100 * iCount);
			// line below is equal to line above
			//  Bracketed this way to prevent possible interger overflow issues with large negative
			//	values that arise when anarchism and similar are evaluated in advanced civilizations
			iTempValue = getNumCities() * ((3 * iHappyValue) / (25 * iCount));
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S anger modifier value %d",
						 kCivic.getDescription(),
						 iTempValue);
			}
			iValue += iTempValue;
		}

	}

	int iHighestReligionCount = ((eBestReligion == NO_RELIGION) ? 0 : getHasReligionCount(eBestReligion));

	//happiness is handled in CvCity::getAdditionalHappinessByCivic
/*
	iValue += (getCivicPercentAnger(eCivic, true) / 10);

	iTempValue = kCivic.getHappyPerMilitaryUnit() * 3;
	if (iTempValue != 0)
	{
		iValue += (getNumCities() * 9 * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
	}

	iTempValue = kCivic.getLargestCityHappiness();
	if (iTempValue != 0)
	{
		iValue += (12 * std::min(getNumCities(), GC.getWorldInfo(GC.getMap().getWorldSize()).getTargetNumCities()) * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
	}

	if (kCivic.getWarWearinessModifier() != 0)
	{
		int iAngerPercent = getWarWearinessPercentAnger();
		int iPopulation = 3 + (getTotalPopulation() / std::max(1, getNumCities()));

		int iTempValue = (-kCivic.getWarWearinessModifier() * iAngerPercent * iPopulation) / (GC.getPERCENT_ANGER_DIVISOR() * 100);
		if (iTempValue != 0)
		{
			iValue += (11 * getNumCities() * ((isCivic(eCivic)) ? -AI_getHappinessWeight(-iTempValue, 1) : AI_getHappinessWeight(iTempValue, 1) )) / 100;
		}
	}

	if (!kCivic.isStateReligion() && !isStateReligion())
	{
		iHighestReligionCount = 0;
	}

	iValue += (kCivic.getNonStateReligionHappiness() * (countTotalHasReligion() - iHighestReligionCount) * 5);
	iValue += (kCivic.getStateReligionHappiness() * iHighestReligionCount * 4);
*/

	if (kCivic.isAnyBuildingHappinessChange())
	{
		for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
		{
			iTempValue = kCivic.getBuildingHappinessChanges(iI);
			if (iTempValue != 0 && !isLimitedWonder((BuildingTypes)iI) && !pTeam.isObsoleteBuilding((BuildingTypes)iI))
			{
				const CvBuildingInfo& buildingInfo = GC.getBuildingInfo((BuildingTypes)iI);
				if (buildingInfo.getPrereqAndTech() == NO_TECH || GC.getTechInfo((TechTypes)buildingInfo.getPrereqAndTech()).getEra() <= getCurrentEra())
				{
					//+0.5 per city that does not yet have that building
					iTempValue = iTempValue * (getNumCities() - getBuildingCount((BuildingTypes)iI)) / 2;
					if (gPlayerLogLevel > 2 && iTempValue != 0)
					{
						logBBAI("Civic %S nat wonder happiness change value %d", kCivic.getDescription(), iTempValue);
					}
					iValue += iTempValue;
				}
			}
		}
	}

	//#2: Health
	if ((getNumCities() > 0) &&
		(kCivic.isNoUnhealthyPopulation() || kCivic.isBuildingOnlyHealthy()
			|| kCivic.getExtraHealth() != 0 || kCivic.isAnyBuildingHealthChange()))
	{
		//int CvPlayerAI::AI_getHealthWeight(int iHealth, int iExtraPop) const

		int iExtraPop = 1;
		int iCount = 0;

		//if (0 == iHealth)
		//{
		//	iHealth = 1;
		//}

		int iCivicsNoUnhealthyPopulationCountNow = 0;
		int iCivicsNoUnhealthyPopulationCountThen = 0;
		int iCivicsBuildingOnlyHealthyCountNow = 0;
		int iCivicsBuildingOnlyHealthyCountThen = 0;

		if (kCivic.isNoUnhealthyPopulation())
		{
			iCivicsNoUnhealthyPopulationCountThen++;
		}
		if (kCivic.isBuildingOnlyHealthy())
		{
			iCivicsBuildingOnlyHealthyCountThen++;
		}

		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			const CivicTypes eTempCivic = ((paeSelectedCivics == NULL) ? getCivics((CivicOptionTypes)iI) : paeSelectedCivics[iI]);
			if (eTempCivic != NO_CIVIC)
			{
				const CvCivicInfo& kTempCivic = GC.getCivicInfo(eTempCivic);
				if (kTempCivic.getCivicOptionType() == iI)
				{
					if (bCivicOptionVacuum)
						continue;
					else
					{
						if (kTempCivic.isNoUnhealthyPopulation())
						{
							iCivicsNoUnhealthyPopulationCountNow++;
						}
						if (kTempCivic.isBuildingOnlyHealthy())
						{
							iCivicsBuildingOnlyHealthyCountNow++;
						}
					}
				}
				else
				{
					if (kTempCivic.isNoUnhealthyPopulation())
					{
						iCivicsNoUnhealthyPopulationCountNow++;
						iCivicsNoUnhealthyPopulationCountThen++;
					}
					if (kTempCivic.isBuildingOnlyHealthy())
					{
						iCivicsBuildingOnlyHealthyCountNow++;
						iCivicsBuildingOnlyHealthyCountThen++;
					}
				}
			}
		}

		int iHealthValue = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityHealth = pLoopCity->goodHealth() - pLoopCity->badHealth(false, iExtraPop);

			int iGoodHealthFromOtherCivics = 0;
			int iBadHealthFromOtherCivics = 0;
			int iGood; int iBad; int iBadBuilding;
			int iGoodFromNoUnhealthyPopulation = 0;
			int iGoodFromBuildingOnlyHealthy = 0;

			//Health calculation (iGood encludes effects from NoUnhealthyPopulation and BuildingOnlyHealthy)
			iGood = 0; iBad = 0; iBadBuilding = 0;
			/*int iHealth =*/ pLoopCity->getAdditionalHealthByCivic(eCivic, iGood, iBad, iBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, iCivicsNoUnhealthyPopulationCountNow, iCivicsBuildingOnlyHealthyCountNow);

			if (iGood == 0 && iBad == 0)
				continue;

			int iTempAdditionalHealthByPlayerBuildingOnlyHealthy = 0;

			if (!bCivicOptionVacuum)
			{
				int iTempGood = 0; int iTempBad = 0; int iTempBadBuilding = 0;
				iCityHealth -= pLoopCity->getAdditionalHealthByCivic(eCivicOptionCivic, iTempGood, iTempBad, iTempBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, 0, 0);
				iTempAdditionalHealthByPlayerBuildingOnlyHealthy -= iTempBadBuilding;
			}
			for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
			{
				if (kCivic.getCivicOptionType() == iI)
					continue;
				CivicTypes eOtherCivic = ((paeSelectedCivics == NULL) ? getCivics((CivicOptionTypes)iI) : paeSelectedCivics[iI]);
				if (eOtherCivic != NULL && eOtherCivic != NO_CIVIC)
				{
					//iGood = 0; iBad = 0; iBadBuilding = 0;
					//int iTempHealth = pLoopCity->getAdditionalHealthByCivic(eOtherCivic, iGood, iBad, iBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, iIgnoreNoUnhealthyPopulationCount, iIgnoreBuildingOnlyHealthyCount);
					//if (iGood > 0)
					//{
					//	iGoodHealthFromOtherCivics += iGood;
					//}
					//if (iBad > 0)
					//{
					//	iBadHealthFromOtherCivics -= iBad; //negative values
					//}
					int iTempBadBuilding = 0;
					pLoopCity->getAdditionalHealthByCivic(eOtherCivic, iGoodHealthFromOtherCivics, iBadHealthFromOtherCivics, iTempBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, 0, 0);
				}
				iBadHealthFromOtherCivics = -iBadHealthFromOtherCivics; //negative values
			}

			//free iCityHealth from all current civic health effects
			iCityHealth -= (iGoodHealthFromOtherCivics + iBadHealthFromOtherCivics); //does not include effects from NoUnhealthyPopulation or BuildingOnlyHealthy
			if (iCivicsNoUnhealthyPopulationCountNow > 0)
			{
				iGoodFromNoUnhealthyPopulation = pLoopCity->getAdditionalHealthByPlayerNoUnhealthyPopulation(iExtraPop, iCivicsNoUnhealthyPopulationCountNow);
				iCityHealth -= iGoodFromNoUnhealthyPopulation;
			}
			if (iCivicsBuildingOnlyHealthyCountNow > 0)
			{
				iGoodFromBuildingOnlyHealthy = pLoopCity->getAdditionalHealthByPlayerBuildingOnlyHealthy(iCivicsBuildingOnlyHealthyCountNow);
				iCityHealth -= iGoodFromBuildingOnlyHealthy;
			}

			//Health calculation
			//iGood = 0; iBad = 0; iBadBuilding = 0;
			//int iHealth = pLoopCity->getAdditionalHealthByCivic(eCivic, iGood, iBad, iBadBuilding, false, iExtraPop, /* bCivicOptionVacuum */ true, iIgnoreNoUnhealthyPopulationCount, iIgnoreBuildingOnlyHealthyCount);
			if (kCivic.isBuildingOnlyHealthy())
			{
				//iHealth += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;
				iGood += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;
			}

			int iBadTotal = -iBad;
			int iGoodTotal = iGood;

			if (kCivic.isNoUnhealthyPopulation())
			{
				iGoodFromNoUnhealthyPopulation = pLoopCity->getAdditionalHealthByPlayerNoUnhealthyPopulation(iExtraPop, iCivicsNoUnhealthyPopulationCountNow);
			}

			if (kCivic.isBuildingOnlyHealthy())
			{
				iGoodFromBuildingOnlyHealthy = pLoopCity->getAdditionalHealthByPlayerBuildingOnlyHealthy(iCivicsBuildingOnlyHealthyCountNow);
			}
			iTempAdditionalHealthByPlayerBuildingOnlyHealthy += iBadBuilding;
			iGoodFromBuildingOnlyHealthy += iTempAdditionalHealthByPlayerBuildingOnlyHealthy;

			iBadTotal += iBadHealthFromOtherCivics;
			iGoodTotal += iGoodHealthFromOtherCivics;
			if (iCivicsNoUnhealthyPopulationCountThen > 0 && !kCivic.isNoUnhealthyPopulation())
				iGoodTotal += iGoodFromNoUnhealthyPopulation;
			if (iCivicsBuildingOnlyHealthyCountThen > 0 && !kCivic.isBuildingOnlyHealthy())
				iGoodTotal += iGoodFromBuildingOnlyHealthy;

			if (iGood > 0)
			{
				//add new civic health effects to iHealthNow/Then
				int iHealthNow = iCityHealth + iBadTotal;
				int iHealthThen = iCityHealth + iGoodTotal + iBadTotal;

				//Fuyu: max health 8
				iHealthNow = std::min(8, iHealthNow);
				iHealthThen = std::min(8, iHealthThen);

				//Integration
				iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
				if (iBadTotal > 0)
				{
					iHealthNow -= iBadTotal;
					iHealthThen -= iBadTotal;

					//Fuyu: max health 8
					iHealthNow = std::min(8, iHealthNow);
					iHealthThen = std::min(8, iHealthThen);

					iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
					iTempValue /= 2;
				}

				//iTempValue = (iTempValue * iGood) / (iGood + iGoodHealthFromOtherCivics);
				int iTempValueFromNoUnhealthyPopulation = 0;
				int iTempValueFromBuildingOnlyHealthy = 0;
				int iTempValueFromRest;
				int iGoodFromRest = iGood;
				if (kCivic.isNoUnhealthyPopulation() && iCivicsNoUnhealthyPopulationCountThen > 1)
				{
					iTempValueFromNoUnhealthyPopulation = (iTempValue * iGoodFromNoUnhealthyPopulation) / (iCivicsNoUnhealthyPopulationCountThen * iGoodTotal);
					iGoodFromRest -= iGoodFromNoUnhealthyPopulation;
				}
				if (kCivic.isBuildingOnlyHealthy() && iCivicsBuildingOnlyHealthyCountThen > 1)
				{
					iTempValueFromBuildingOnlyHealthy = (iTempValue * iGoodFromBuildingOnlyHealthy) / (iCivicsBuildingOnlyHealthyCountThen * iGoodTotal);
					iGoodFromRest -= iGoodFromBuildingOnlyHealthy;
				}
				iTempValueFromRest = (iTempValue * iGood) / iGoodTotal;

				iTempValue = iTempValueFromNoUnhealthyPopulation + iTempValueFromBuildingOnlyHealthy + iTempValueFromRest;

				iHealthValue += std::max(0, iTempValue) * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);
			}
			if (iBad > 0)
			{
				//add new civic health effects to iHealthNow/Then
				int iHealthNow = iCityHealth;
				int iHealthThen = iCityHealth + iBadTotal;

				//Fuyu: max health 8
				iHealthNow = std::min(8, iHealthNow);
				iHealthThen = std::min(8, iHealthThen);

				//Integration
				int iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
				if (iGoodTotal > 0)
				{
					iHealthNow += iGoodTotal;
					iHealthThen += iGoodTotal;

					//Fuyu: max health 8
					iHealthNow = std::min(8, iHealthNow);
					iHealthThen = std::min(8, iHealthThen);

					iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
					iTempValue /= 2;
				}


				iTempValue = (iTempValue * iBad) / std::max(1, (-iBadTotal)); //-iBadTotal == iBad - iBadHealthFromOtherCivics

				iHealthValue += std::min(0, iTempValue) * /* weighting */ (pLoopCity->getPopulation() + iExtraPop + 2);
			}

			//iCount++;
			iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
			//if (iCount > 6)
			//{
			//	break;
			//}
		}
		//return (0 == iCount) ? 50*iHealth : iHealthValue / iCount;

		if (iCount <= 0)
		{
			//iValue += (getNumCities() * 6 * 50*iHealth) / 100; //always 0 because getNumCities() is 0
		}
		else
		{
			//iValue += (getNumCities() * 6 * iHealthValue) / (100 * iCount);
			// line below is equal to line above
			iTempValue = (getNumCities() * 3 * iHealthValue) / (50 * iCount);
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S health value %d",
						 kCivic.getDescription(),
						 iTempValue);
			}
			iValue += iTempValue;
		}
	}


	//health is handled in CvCity::getAdditionalHealthByCivic
/*
	iValue += ((kCivic.isNoUnhealthyPopulation()) ? (getTotalPopulation() / 3) : 0);
	iValue += ((kCivic.isBuildingOnlyHealthy()) ? (getNumCities() * 3) : 0);

	if (kCivic.getExtraHealth() != 0)
	{
		iValue += (getNumCities() * 6 * ((isCivic(eCivic)) ? -AI_getHealthWeight(-kCivic.getExtraHealth(), 1) : AI_getHealthWeight(kCivic.getExtraHealth(), 1) )) / 100;
	}
*/

	iTempValue = 0;
	if (kCivic.isAnyBuildingHealthChange())
	{
		for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
		{
			const int iHealthChange = kCivic.getBuildingHealthChanges(iI);

			if (iHealthChange != 0 && !isLimitedWonder((BuildingTypes)iI))
			{
				//+0.5 per city that does not yet have that building
				iTempValue += iHealthChange * std::min(getNumCities(), getNumCities() - getBuildingCount((BuildingTypes)iI)) / 2;
			}
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S building health value %d", kCivic.getDescription(), iTempValue);
	}
	iValue += iTempValue;
	//#2: Health - end

	//#3: Trade
	{
		int iTempNoForeignTradeCount = getNoForeignTradeCount();

		if (!bCivicOptionVacuum)
		{
			iTempNoForeignTradeCount -= (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isNoForeignTrade());
		}
		const int iConnectedForeignCities = countPotentialForeignTradeCitiesConnected();
		int iTempValue = 0;
		if (iTempNoForeignTradeCount > 0)
		{
			if (kCivic.isNoForeignTrade())
			{
				iTempValue -= iConnectedForeignCities * 3 / (1 + iTempNoForeignTradeCount);
				// No additional negative value from NoForeignTrade
				iTempValue += kCivic.getTradeRoutes() * getNumCities() * 2;
			}
			else
			{
				//kCivic.getTradeRoutes() should be 0
				//FAssertMsg(kCivic.getTradeRoutes() == 0, "kCivic.getTradeRoutes() is supposed to be 0 if kPlayer.isNoForeignTrade() is true");
				iTempValue += kCivic.getTradeRoutes() * (std::max(0, iConnectedForeignCities - getNumCities() * 3) * 6 + getNumCities() * 2);
			}
		}
		else if (kCivic.isNoForeignTrade())
		{
			iTempValue -= iConnectedForeignCities * 3;
			iTempValue += kCivic.getTradeRoutes() * getNumCities() * 2;
		}
		else
		{
			iTempValue += kCivic.getTradeRoutes() * (std::max(0, iConnectedForeignCities - getNumCities() * 3) * 6 + getNumCities() * 2);
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S foreign trade value %d", kCivic.getDescription(), iTempValue);
		}
		iValue += iTempValue;
	}
	//#3: Trade - end


	//#4: Corporations

	int iCorpMaintenanceMod = kCivic.getCorporationMaintenanceModifier();
	iTempValue = 0;
	if (kCivic.isNoCorporations() || kCivic.isNoForeignCorporations() || iCorpMaintenanceMod != 0)
	{
		int iHQCount = 0;
		int iOwnCorpCount = 0;
		int iForeignCorpCount = 0;
		for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
		{
			if (pTeam.hasHeadquarters((CorporationTypes)iCorp))
			{
				iHQCount++;
				iOwnCorpCount += countCorporations((CorporationTypes)iCorp);
			}
			else
			{
				iForeignCorpCount += countCorporations((CorporationTypes)iCorp);
			}
		}

		int iTempNoForeignCorporationsCount = 0;
		iTempNoForeignCorporationsCount += getNoForeignCorporationsCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC ? GC.getCivicInfo(eCivicOptionCivic).isNoForeignCorporations() : false)
			{
				iTempNoForeignCorporationsCount--;
			}
		}
		iTempNoForeignCorporationsCount = std::max(0, iTempNoForeignCorporationsCount);

		int iTempNoCorporationsCount = 0;
		iTempNoCorporationsCount += getNoCorporationsCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC ? GC.getCivicInfo(eCivicOptionCivic).isNoCorporations() : false)
			{
				iTempNoCorporationsCount--;
			}
		}
		iTempNoCorporationsCount = std::max(0, iTempNoCorporationsCount);

		int iTempCorporationValue = 0;
		if (kCivic.isNoCorporations())
		{
			iTempCorporationValue = 0;
			iTempCorporationValue -= iHQCount * (40 + 3 * getNumCities());
			iTempValue += iTempCorporationValue / (1 + iTempNoCorporationsCount);

			if (kCivic.isNoForeignCorporations()) // ROM Planned denies all corporations
			{
				iTempCorporationValue = iForeignCorpCount * 3;
				iTempValue += iTempCorporationValue / (2 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);
			}
			else if (iTempNoForeignCorporationsCount > 0)
			{
				iTempCorporationValue = -iForeignCorpCount * 3;
				iTempValue += iTempCorporationValue / (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);
			}

			//FAssertMsg((iCorpMaintenanceMod== 0), "NoCorporation civics are not supposed to be have a maintenace modifier");
			//subtracting value from already applied kPlayer.getCorporationMaintenanceModifier()
			if ((getCorporationMaintenanceModifier() + iCorpMaintenanceMod) != 0)
			{
				iTempCorporationValue = 0;
				iTempCorporationValue -= (-(getCorporationMaintenanceModifier() + iCorpMaintenanceMod) * (iHQCount * (25 + getNumCities() * 2) + iOwnCorpCount * 7)) / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + iTempNoCorporationsCount));

				iTempCorporationValue = 0;
				iTempCorporationValue -= (-(getCorporationMaintenanceModifier() + iCorpMaintenanceMod) * (iForeignCorpCount * 7)) / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + ((kCivic.isNoForeignCorporations()) ? 1 : 0) + iTempNoForeignCorporationsCount + iTempNoCorporationsCount));
			}
		}
		else if (kCivic.isNoForeignCorporations())
		{
			iTempCorporationValue = iForeignCorpCount * 3;
			iTempValue += iTempCorporationValue / (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount);

			if ((getCorporationMaintenanceModifier() + iCorpMaintenanceMod) != 0)
			{
				iTempCorporationValue = 0;
				iTempCorporationValue -= -(getCorporationMaintenanceModifier() + iCorpMaintenanceMod) * iForeignCorpCount * 7 / 25;
				iTempValue += iTempCorporationValue / (2 * (1 + iTempNoForeignCorporationsCount + iTempNoCorporationsCount));
			}
		}

		if (iCorpMaintenanceMod != 0)
		{
			iTempCorporationValue = 0;
			iTempCorporationValue += (-iCorpMaintenanceMod * (iForeignCorpCount * 7)) / 25;
			if (kCivic.isNoForeignCorporations() || kCivic.isNoCorporations() || iTempNoForeignCorporationsCount > 0 || iTempNoCorporationsCount > 0)
				iTempCorporationValue /= 2;
			iTempValue += iTempCorporationValue;

			iTempCorporationValue = 0;
			iTempCorporationValue += (-iCorpMaintenanceMod * (iHQCount * (25 + getNumCities() * 2) + iOwnCorpCount * 7)) / 25;
			if (kCivic.isNoCorporations() || iTempNoCorporationsCount > 0)
				iTempCorporationValue /= 2;
			iTempValue += iTempCorporationValue;
		}
	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S corporation value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;


	/*
		if (kCivic.isNoCorporations())
		{
			iValue -= countHeadquarters() * (40 + 3 * getNumCities());
		}
		if (kCivic.isNoForeignCorporations())
		{
			for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
			{
				if (!pTeam.hasHeadquarters((CorporationTypes)iCorp))
				{
					iValue += countCorporations((CorporationTypes)iCorp) * 3;
				}
			}
		}
		if (iCorpMaintenanceMod != 0)
		{
			int iCorpCount = 0;
			int iHQCount = 0;
			for (int iCorp = 0; iCorp < GC.getNumCorporationInfos(); ++iCorp)
			{
				if (pTeam.hasHeadquarters((CorporationTypes)iCorp))
				{
					iHQCount++;
				}
				iCorpCount += countCorporations((CorporationTypes)iCorp);
			}
			iValue += (-iCorpMaintenanceMod * (iHQCount * (25 + getNumCities() * 2) + iCorpCount * 7)) / 25;
		}
	*/
	//#4: Corporations - end

	//#5: state religion
	int iStateReligionValue = 0;
	int iTempStateReligionCount = 0;
	iTempStateReligionCount += getStateReligionCount();
	if (!bCivicOptionVacuum)
	{
		iTempStateReligionCount -= ((eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isStateReligion()) ? 1 : 0);
	}

	if (kCivic.isStateReligion() || iTempStateReligionCount > 0)
	{
		if (iHighestReligionCount > 0)
		{
			//iValue += ((kCivic.isNoNonStateReligionSpread() && !isNoNonStateReligionSpread()) ? ((getNumCities() - iHighestReligionCount) * 2) : 0);
			if (kCivic.isNoNonStateReligionSpread())
			{
				int iTempNoNonStateReligionSpreadCount = 0;
				iTempNoNonStateReligionSpreadCount += getNoNonStateReligionSpreadCount();
				if (!bCivicOptionVacuum)
				{
					iTempNoNonStateReligionSpreadCount -= ((eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isNoNonStateReligionSpread()) ? 1 : 0);
				}
				iTempNoNonStateReligionSpreadCount = std::max(0, iTempNoNonStateReligionSpreadCount);

				iStateReligionValue += ((getNumCities() - iHighestReligionCount) * 2) / std::max(1, iTempNoNonStateReligionSpreadCount);
			}
			//iValue += (kCivic.getStateReligionHappiness() * iHighestReligionCount * 4);
			iStateReligionValue += ((kCivic.getStateReligionGreatPeopleRateModifier() * iHighestReligionCount) / 20);
			iStateReligionValue += (kCivic.getStateReligionGreatPeopleRateModifier() / 4);
			iStateReligionValue += ((kCivic.getStateReligionUnitProductionModifier() * iHighestReligionCount) / 4);
			iStateReligionValue += ((kCivic.getStateReligionBuildingProductionModifier() * iHighestReligionCount) / 3);
			iStateReligionValue += (kCivic.getStateReligionFreeExperience() * iHighestReligionCount * ((bWarPlan) ? 6 : 2));

			int iTempReligionValue = 0;

			if (kCivic.isStateReligion())
			{
				iTempReligionValue += iHighestReligionCount;

				// Value civic based on current gains from having a state religion
				for (int iI = 0; iI < GC.getNumVoteSourceInfos(); ++iI)
				{
					if (GC.getGame().isDiploVote((VoteSourceTypes)iI))
					{
						const ReligionTypes eReligion = GC.getGame().getVoteSourceReligion((VoteSourceTypes)iI);

						if (NO_RELIGION != eReligion && eReligion == eBestReligion)
						{
							// Are we leader of AP?
							if (getTeam() == GC.getGame().getSecretaryGeneral((VoteSourceTypes)iI))
							{
								iTempReligionValue += 100;
							}

							// Any benefits we get from AP tied to state religion?
							/*
							for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
							{
								iTempValue = iHighestReligionCount*GC.getVoteSourceInfo((VoteSourceTypes)iI).getReligionYield(iYield);

								iTempValue *= AI_yieldWeight((YieldTypes)iYield);
								iTempValue /= 100;

								iTempReligionValue += iTempValue;
							}

							for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
							{
								iTempValue = (iHighestReligionCount*GC.getVoteSourceInfo((VoteSourceTypes)iI).getReligionCommerce(iCommerce))/2;

								iTempValue *= AI_commerceWeight((CommerceTypes)iCommerce);
								iTempValue = 100;

								iTempReligionValue += iTempValue;
							}
							*/
						}
					}
				}

				// Value civic based on wonders granting state religion boosts
				for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
				{
					iTempValue = (iHighestReligionCount * getStateReligionBuildingCommerce((CommerceTypes)iCommerce)) / 2;

					iTempValue *= AI_commerceWeight((CommerceTypes)iCommerce);
					iTempValue /= 100;

					iTempReligionValue += iTempValue;
				}
			}

			iStateReligionValue += iTempReligionValue / std::max(1, iTempStateReligionCount);
		}
	}
	if (gPlayerLogLevel > 2 && iStateReligionValue != 0)
	{
		logBBAI("Civic %S state religion value %d",
				 kCivic.getDescription(),
				 iStateReligionValue);
	}
	iValue += iStateReligionValue;
	//#5: state religion - end


	//#6: other possibly non-constant factors

	//iValue += ((kCivic.isMilitaryFoodProduction()) ? 0 : 0);
	if (kCivic.isMilitaryFoodProduction())
	{

		int iTempMilitaryFoodProductionCount = 0;
		iTempMilitaryFoodProductionCount += getMilitaryFoodProductionCount();
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isMilitaryFoodProduction())
			{
				iTempMilitaryFoodProductionCount--;
			}
		}
		iTempMilitaryFoodProductionCount = std::max(0, iTempMilitaryFoodProductionCount);

		if (AI_isDoStrategy(AI_STRATEGY_LAST_STAND)) //when is it wanted, and how much is it worth in those cases?
		{
			iTempValue = getNumCities() / (1 + iTempMilitaryFoodProductionCount);
		}
		else
		{
			//We want to grow so we don't want this
			iTempValue = -10 * getNumCities() / (1 + std::max(0, getMilitaryFoodProductionCount()));
		}
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S military food production value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	for (int iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (kCivic.isHurry(iI))
		{
			iTempValue = 0;

			if (GC.getHurryInfo((HurryTypes)iI).getGoldPerProduction() > 0)
			{
				iTempValue += ((((AI_avoidScience()) ? 50 : 25) * getNumCities()) / GC.getHurryInfo((HurryTypes)iI).getGoldPerProduction());
			}
			iTempValue += (GC.getHurryInfo((HurryTypes)iI).getProductionPerPopulation() * getNumCities() * (bWarPlan ? 2 : 1)) / 5;

			int iTempHurryCount = 0;

			iTempHurryCount += getHurryCount((HurryTypes)iI);
			if (!bCivicOptionVacuum)
			{
				if (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isHurry(iI))
				{
					iTempHurryCount--;
				}
			}
			iTempHurryCount = std::max(0, iTempHurryCount);

			iTempValue = iTempValue / (1 + iTempHurryCount);
			if (gPlayerLogLevel > 2 && iTempValue != 0)
			{
				logBBAI("Civic %S hurry value %d",
						 kCivic.getDescription(),
						 iTempValue);
			}
			iValue += iTempValue;
		}
	}

	iTempValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (kCivic.isSpecialBuildingNotRequired(iI))
		{
			int iTempSpecialBuildingNotRequiredCount = 0;
			iTempSpecialBuildingNotRequiredCount += getSpecialBuildingNotRequiredCount((SpecialBuildingTypes)iI);
			if (!bCivicOptionVacuum)
			{
				if (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isSpecialBuildingNotRequired(iI))
				{
					iTempSpecialBuildingNotRequiredCount--;
				}
			}
			iTempSpecialBuildingNotRequiredCount = std::max(0, iTempSpecialBuildingNotRequiredCount);
			iTempValue += ((getNumCities() / 2) + 1) / (1 + iTempSpecialBuildingNotRequiredCount); // XXX
		}

	}
	if (gPlayerLogLevel > 2 && iTempValue != 0)
	{
		logBBAI("Civic %S special buildings value %d",
				 kCivic.getDescription(),
				 iTempValue);
	}
	iValue += iTempValue;

	int iTempSpecialistValue = 0;
	for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
	{
		iTempValue = 0;
		if (kCivic.isSpecialistValid(iI))
		{
			iTempValue += ((getNumCities() * (bCultureVictory3 ? 10 : 1)) + 6);
		}
		int iTempSpecialistValidCount = 0;
		iTempSpecialistValidCount += getSpecialistValidCount((SpecialistTypes)iI);
		if (!bCivicOptionVacuum)
		{
			if (eCivicOptionCivic != NO_CIVIC && GC.getCivicInfo(eCivicOptionCivic).isSpecialistValid(iI))
			{
				iTempSpecialistValidCount--;
			}
		}
		iTempSpecialistValidCount = std::max(0, iTempSpecialistValidCount);

		iValue += (iTempValue / 2) / (1 + iTempSpecialistValidCount);
	}
	if (gPlayerLogLevel > 2 && iTempSpecialistValue != 0)
	{
		logBBAI("Civic %S specialist value %d",
				 kCivic.getDescription(),
				 iTempSpecialistValue);
	}
	iValue += iTempSpecialistValue;

	//#7: final modifiers
	if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() == eCivic)
	{
		iTempValue = iValue;
		if (!kCivic.isStateReligion() || iHighestReligionCount > 0)
		{
			iTempValue *= 5;
			iTempValue /= 4;
			iTempValue += 6 * getNumCities();
			iTempValue += 20;
		}

		iTempValue -= iValue;
		if (gPlayerLogLevel > 2 && iTempValue != 0)
		{
			logBBAI("Civic %S favourite civic value %d",
					 kCivic.getDescription(),
					 iTempValue);
		}
		iValue += iTempValue;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2) && (kCivic.isNoNonStateReligionSpread()))
	{
		//is this really necessary, even if already running culture3/4 ?
		iValue /= 10;
	}

	m_aiCivicValueCache[eCivic + (bCivicOptionVacuum ? 0 : GC.getNumCivicInfos())] = iValue;

	return iValue;
}


int CvPlayerAI::AI_RevCalcCivicRelEffect(CivicTypes eCivic) const
{
	PROFILE_EXTRA_FUNC();
	if (isNPC())
		return 0;
	if (!isAlive())
		return 0;
	if (getNumCities() == 0)
		return 0;

	int iTotalScore = 0;

	if (GC.getCivicInfo(eCivic).isStateReligion())
	{
		int iRelScore = 0;

		float fRelGoodMod = GC.getCivicInfo(eCivic).getRevIdxGoodReligionMod();
		float fRelBadMod = GC.getCivicInfo(eCivic).getRevIdxBadReligionMod();
		int iHolyCityGood = GC.getCivicInfo(eCivic).getRevIdxHolyCityGood();
		int iHolyCityBad = GC.getCivicInfo(eCivic).getRevIdxHolyCityBad();

		ReligionTypes eStateReligion = getStateReligion();

		if (eStateReligion == NO_RELIGION)
		{
			eStateReligion = getLastStateReligion();
		}
		if (eStateReligion == NO_RELIGION)
		{
			eStateReligion = GET_PLAYER(getID()).AI_findHighestHasReligion();
		}
		if (eStateReligion == NO_RELIGION)
		{
			return 0;
		}

		CvCity* pHolyCity = GC.getGame().getHolyCity(eStateReligion);

		foreach_(const CvCity * pLoopCity, cities())
		{
			float fCityStateReligion = 0;
			float fCityNonStateReligion = 0;

			if (pLoopCity->isHasReligion(eStateReligion))
			{
				fCityStateReligion += 4;
			}
			for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
			{
				if ((pLoopCity->isHasReligion((ReligionTypes)iI)) && !(eStateReligion == iI))
				{
					if (fCityNonStateReligion <= 4)
					{
						fCityNonStateReligion += 2.5;
					}
					else
					{
						fCityNonStateReligion += 1;
					}
				}
			}
			if (pLoopCity->isHolyCity())
			{
				if (pLoopCity->isHolyCity(eStateReligion))
				{
					fCityStateReligion += 5;
				}
				else
				{
					fCityNonStateReligion += 4;
				}
			}
			int iLiberalism = GC.getInfoTypeForString("TECH_LIBERALISM");
			int iSciMethod = GC.getInfoTypeForString("TECH_SCIENTIFIC_METHOD");
			bool bHeathens = false;
			if (!(GET_TEAM(getTeam()).isHasTech((TechTypes)iLiberalism)) && (pLoopCity->isHasReligion(eStateReligion)))
			{
				if (pHolyCity != NULL)
				{
					PlayerTypes eHolyCityOwnerID = pHolyCity->getOwner();
					if (getID() == eHolyCityOwnerID)
					{
						fCityStateReligion += iHolyCityGood;
					}
					else
					{
						if (GET_PLAYER(eHolyCityOwnerID).getStateReligion() != eStateReligion)//heathens!
						{
							bHeathens = true;
						}
					}
				}
			}

			int iRelBadEffect = (int)floor((fCityNonStateReligion * (1 + fRelBadMod)) + .5);
			int iRelGoodEffect = (int)floor((fCityStateReligion * (1 + fRelGoodMod)) + .5);

			if (GET_TEAM(getTeam()).isAtWar())
			{
				iRelGoodEffect = (int)floor((iRelGoodEffect * 1.5) + .5);
			}

			int iNetCivicRelEffect = iRelBadEffect - iRelGoodEffect;
			if (bHeathens)
			{
				iNetCivicRelEffect += iHolyCityBad;
			}

			if (GET_TEAM(getTeam()).isHasTech((TechTypes)iSciMethod))
			{
				iNetCivicRelEffect /= 3;
			}
			else if (GET_TEAM(getTeam()).isHasTech((TechTypes)iLiberalism))
			{
				iNetCivicRelEffect /= 2;
			}
			int iRevIdx = pLoopCity->getRevolutionIndex();
			iRevIdx = std::max(iRevIdx - 300, 100);
			float fCityReligionScore = iNetCivicRelEffect * (((float)iRevIdx) / 600);
			iRelScore += (int)(floor(fCityReligionScore));
		}//end of each city loop

		iRelScore *= 3;
		iTotalScore -= iRelScore;
	}//end of if eCivic isStateRel

	if (GC.getCivicInfo(eCivic).getNonStateReligionHappiness() > 0)
	{
		int iCivicScore = 0;

		foreach_(const CvCity * pLoopCity, cities())
		{
			int iCityScore = GC.getCivicInfo(eCivic).getNonStateReligionHappiness() * pLoopCity->getReligionCount();

			int iRevIdx = pLoopCity->getRevolutionIndex();
			iRevIdx = std::max(iRevIdx - 300, 100);

			iCityScore *= iRevIdx;
			iCityScore /= (pLoopCity->angryPopulation() > 0) ? 500 : 700;

			iCivicScore += iCityScore;
		}

		iTotalScore += iCivicScore;
	}

	return iTotalScore;
}


ReligionTypes CvPlayerAI::AI_bestReligion() const
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = 0;
	ReligionTypes eBestReligion = NO_RELIGION;
	const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();

	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (canDoReligion((ReligionTypes)iI))
		{
			int iValue = AI_religionValue((ReligionTypes)iI);

			if (getStateReligion() == ((ReligionTypes)iI))
			{
				iValue *= 4;
				iValue /= 3;
			}

			if (eFavorite == iI)
			{
				iValue *= 5;
				iValue /= 4;
			}

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				eBestReligion = ((ReligionTypes)iI);
			}
		}
	}

	if ((NO_RELIGION == eBestReligion) || AI_isDoStrategy(AI_STRATEGY_MISSIONARY))
	{
		return eBestReligion;
	}

	const int iBestCount = getHasReligionCount(eBestReligion);
	const int iPurityPercent = (iBestCount * 100) / std::max(1, countTotalHasReligion());

	if (iPurityPercent < 49)
	{
		const int iSpreadPercent = (iBestCount * 100) / std::max(1, getNumCities());

		if (iSpreadPercent > ((eBestReligion == eFavorite) ? 65 : 75)
		&& iPurityPercent > ((eBestReligion == eFavorite) ? 25 : 32))
		{
			return eBestReligion;
		}
		return NO_RELIGION;
	}
	return eBestReligion;
}


int CvPlayerAI::AI_religionValue(ReligionTypes eReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (getHasReligionCount(eReligion) == 0)
	{
		return 0;
	}
	int iValue = GC.getGame().countReligionLevels(eReligion);

	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isHasReligion(eReligion))
		{
			iValue += pLoopCity->getPopulation();
		}
	}

	CvCity* pHolyCity = GC.getGame().getHolyCity(eReligion);
	if (pHolyCity)
	{
		bool bOurHolyCity = pHolyCity->getOwner() == getID();
		bool bOurTeamHolyCity = pHolyCity->getTeam() == getTeam();

		if (bOurHolyCity || bOurTeamHolyCity)
		{
			int iCommerceCount = 0;

			foreach_(const BuildingTypes eTypeX, pHolyCity->getHasBuildings())
			{
				if (pHolyCity->isDisabledBuilding(eTypeX))
				{
					continue;
				}
				for (int iJ = 0; iJ < NUM_COMMERCE_TYPES; iJ++)
				{
					if (GC.getBuildingInfo(eTypeX).getGlobalReligionCommerce() == eReligion)
					{
						iCommerceCount += GC.getReligionInfo(eReligion).getGlobalReligionCommerce((CommerceTypes)iJ);
					}
				}
			}

			if (bOurHolyCity)
			{
				iValue *= (3 + iCommerceCount);
				iValue /= 2;
			}
			else if (bOurTeamHolyCity)
			{
				iValue *= (4 + iCommerceCount);
				iValue /= 3;
			}
		}
	}

	int iTempValueModifier = 100;
	//Consider what kind of delay a change in Religion would mean
	if (getReligionAnarchyLength() != 0 && getStateReligion() != eReligion)
	{
		iTempValueModifier -= (getReligionAnarchyLength() * (10 - (int)GC.getGame().getGameSpeedType()));
	}
	iValue *= iTempValueModifier;
	iValue /= 100;

	return iValue;
}


ReligionTypes CvPlayerAI::AI_findHighestHasReligion()
{
	PROFILE_EXTRA_FUNC();
	int iValue;
	int iBestValue;
	int iI;
	ReligionTypes eMostReligion = NO_RELIGION;

	iBestValue = 0;

	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		iValue = getHasReligionCount((ReligionTypes)iI);

		if (iValue > iBestValue)
		{
			iBestValue = iValue;
			eMostReligion = (ReligionTypes)iI;
		}
	}
	return eMostReligion;
}


EspionageMissionTypes CvPlayerAI::AI_bestPlotEspionage(CvPlot* pSpyPlot, PlayerTypes& eTargetPlayer, CvPlot*& pPlot, int& iData) const
{
	PROFILE_EXTRA_FUNC();
	//ooookay what missions are possible

	FAssert(pSpyPlot != NULL);

	pPlot = NULL;
	iData = -1;

	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	//int iBestValue = 0;
	int iBestValue = 20;

	if (pSpyPlot->isNPC())
	{
		return eBestMission;
	}

	if (pSpyPlot->isOwned())
	{
		if (pSpyPlot->getTeam() != getTeam())
		{
			if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE) && (GET_TEAM(getTeam()).AI_getWarPlan(pSpyPlot->getTeam()) != NO_WARPLAN || AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1)))
			{
				//Destroy Improvement.
				if (pSpyPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.isDestroyImprovement())
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}

				//Bribe
				if (pSpyPlot->plotCount(PUF_isOtherTeam, getID(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID()) >= 1)
				{
					if (pSpyPlot->plotCount(PUF_isUnitAIType, UNITAI_WORKER, -1) >= 1)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.getBuyUnitCostFactor() > 0 && GC.getDefineINT("SS_BRIBE"))
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = -1;
								}
							}
						}
					}
				}
			}

			CvCity* pCity = pSpyPlot->getPlotCity();
			if (pCity != NULL)
			{
				for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
				{
					const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
					if (kMissionInfo.isRevolt() || kMissionInfo.isDisablePower() || kMissionInfo.getWarWearinessCounter() > 0 || kMissionInfo.getBuyCityCostFactor() > 0)
					{
						int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eBestMission = (EspionageMissionTypes)iMission;
							eTargetPlayer = pSpyPlot->getOwner();
							pPlot = pSpyPlot;
							iData = -1;
						}
					}
				}
			}
			if (pCity != NULL)
			{
				if (GET_TEAM(getTeam()).AI_getWarPlan(pCity->getTeam()) != NO_WARPLAN)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.isNuke())
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
			}
			if (pCity != NULL)
			{
				if ((pCity->plot()->countTotalCulture() / std::max(1, pCity->plot()->getCulture(getID()))) > 25)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getCityInsertCultureAmountFactor() > 0)
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
			}
			if (pCity != NULL)
			{
				//Something malicious
				if (AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1))
				{
					//Destroy Building.
					if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							if (GC.getEspionageMissionInfo((EspionageMissionTypes)iMission).getDestroyBuildingCostFactor() < 1)
							{
								continue;
							}
							foreach_(const BuildingTypes eTypeX, pCity->getHasBuildings())
							{
								if (pCity->isDisabledBuilding(eTypeX))
								{
									continue;
								}
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, eTypeX);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = eTypeX;
								}
							}
						}
					}

					//Destroy Project
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getDestroyProjectCostFactor() > 0)
						{
							for (int iProject = 0; iProject < GC.getNumProjectInfos(); iProject++)
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, iProject);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = iProject;
								}
							}
						}
					}

					//General dataless city mission.
					if (!AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
							{
								if ((kMissionInfo.getCityPoisonWaterCounter() > 0) || (kMissionInfo.getDestroyProductionCostFactor() > 0)
									|| (kMissionInfo.getStealTreasuryTypes() > 0))
								{
									int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestMission = (EspionageMissionTypes)iMission;
										eTargetPlayer = pSpyPlot->getOwner();
										pPlot = pSpyPlot;
										iData = -1;
									}
								}
							}
						}
					}

					//Disruption suitable for war.
					if (GET_TEAM(getTeam()).isAtWar(pSpyPlot->getTeam()))
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
							if ((kMissionInfo.getCityRevoltCounter() > 0) || (kMissionInfo.getPlayerAnarchyCounter() > 0))
							{
								int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestMission = (EspionageMissionTypes)iMission;
									eTargetPlayer = pSpyPlot->getOwner();
									pPlot = pSpyPlot;
									iData = -1;
								}
							}
						}
					}

					//Assassinate
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.getDestroyUnitCostFactor() > 0 && GC.getDefineINT("SS_ASSASSINATE"))
						{
							SpecialistTypes theGreatSpecialistTarget = (SpecialistTypes)0;

							CvCity* pCity = pSpyPlot->getPlotCity();
							if (NULL != pCity)
							{
								//loop through all great specialist types
								for (int iSpecialist = 7; iSpecialist < GC.getNumSpecialistInfos(); iSpecialist++)
								{
									SpecialistTypes tempSpecialist = (SpecialistTypes)0;
									//does this city contain this great specialist type?
									if (pCity->getFreeSpecialistCount((SpecialistTypes)iSpecialist) > 0)
									{
										//sort who is the most significant great specialist in the city
										//prefer any custom specialist	(SpecialistTypes)>13
										//then great spies				(SpecialistTypes)13
										//then great generals			(SpecialistTypes)12
										//then great engineers			(SpecialistTypes)11
										//then great merchants			(SpecialistTypes)10
										//then great scientists			(SpecialistTypes)9
										//then great artists			(SpecialistTypes)8
										//then great priests			(SpecialistTypes)7
										tempSpecialist = (SpecialistTypes)iSpecialist;
										if (tempSpecialist > theGreatSpecialistTarget)
										{
											theGreatSpecialistTarget = tempSpecialist;
										}
									}
								}
							}

							const int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = theGreatSpecialistTarget;
							}
						}
					}
				}

				//TSHEEP - Counter Espionage (Why the heck don't AIs use this in vanilla?) -
				//Requires either annoyance or memory of past Spy transgression
				if ((AI_getAttitudeWeight(pSpyPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 50 : 0) ||
					AI_getMemoryCount(pSpyPlot->getOwner(), MEMORY_SPY_CAUGHT) > 0) &&
					GET_TEAM(getTeam()).getCounterespionageTurnsLeftAgainstTeam(GET_PLAYER(pSpyPlot->getOwner()).getTeam()) <= 0)
				{
					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);
						if (kMissionInfo.getCounterespionageNumTurns() > 0)
						{
							int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, -1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = -1;
							}
						}
					}
				}
				//TSHEEP End of Counter Espionage

				//Steal Technology
				for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
				{
					if (GC.getEspionageMissionInfo((EspionageMissionTypes)iMission).getBuyTechCostFactor() > 0)
					{
						for (int iTech = 0; iTech < GC.getNumTechInfos(); iTech++)
						{
							const TechTypes eTech = (TechTypes)iTech;
							const int iValue = AI_espionageVal(pSpyPlot->getOwner(), (EspionageMissionTypes)iMission, pSpyPlot, eTech);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestMission = (EspionageMissionTypes)iMission;
								eTargetPlayer = pSpyPlot->getOwner();
								pPlot = pSpyPlot;
								iData = eTech;
							}
						}
					}
				}
			}
		}
	}

	return eBestMission;
}


/// Assigns value to espionage mission against ePlayer at pPlot, where iData can provide additional information about mission.
int CvPlayerAI::AI_espionageVal(PlayerTypes eTargetPlayer, EspionageMissionTypes eMission, const CvPlot* pPlot, int iData) const
{
	PROFILE_FUNC();
	FAssertMsg(pPlot != NULL, "Plot is not allowed to be NULL");

	if (eTargetPlayer == NO_PLAYER)
	{
		return 0;
	}

	const TeamTypes eTargetTeam = GET_PLAYER(eTargetPlayer).getTeam();

	int iCost = getEspionageMissionCost(eMission, eTargetPlayer, pPlot, iData);

	if (!canDoEspionageMission(eMission, eTargetPlayer, pPlot, iData, NULL))
	{
		return 0;
	}

	const bool bMalicious = (AI_getAttitudeWeight(pPlot->getOwner()) < (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 51 : 1) || GET_TEAM(getTeam()).AI_getWarPlan(eTargetTeam) != NO_WARPLAN);

	int64_t iValue = 0;
	if (bMalicious && GC.getEspionageMissionInfo(eMission).isDestroyImprovement())
	{
		if (pPlot->getOwner() == eTargetPlayer)
		{
			ImprovementTypes eImprovement = pPlot->getImprovementType();
			if (eImprovement != NO_IMPROVEMENT)
			{
				BonusTypes eBonus = pPlot->getNonObsoleteBonusType(GET_PLAYER(eTargetPlayer).getTeam());
				if (NO_BONUS != eBonus)
				{
					iValue += GET_PLAYER(eTargetPlayer).AI_bonusVal(eBonus, -1);

					int iTempValue = 0;
					if (NULL != pPlot->getWorkingCity())
					{
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, pPlot->getOwner()) * 2);
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_PRODUCTION, pPlot->getOwner()) * 1);
						iTempValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_COMMERCE, pPlot->getOwner()) * 2);
						iTempValue += GC.getImprovementInfo(eImprovement).getUpgradeTime() / 2;
						iValue += iTempValue;
					}
				}
			}
		}
	}

	if (bMalicious
	&& GC.getEspionageMissionInfo(eMission).getDestroyBuildingCostFactor() > 0
	&& canSpyDestroyBuilding(eTargetPlayer, (BuildingTypes)iData))
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (pCity && pCity->isActiveBuilding((BuildingTypes)iData))
		{
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iData);
			if (kBuilding.getProductionCost() > 1 && !isWorldWonder((BuildingTypes)iData))
			{
				iValue += pCity->AI_buildingValue((BuildingTypes)iData);
				iValue *= 60 + kBuilding.getProductionCost();
				iValue /= 100;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyProjectCostFactor() > 0)
	{
		if (canSpyDestroyProject(eTargetPlayer, (ProjectTypes)iData))
		{
			const CvProjectInfo& kProject = GC.getProjectInfo((ProjectTypes)iData);

			iValue += getProductionNeeded((ProjectTypes)iData) * ((kProject.getMaxTeamInstances() == 1) ? 3 : 2);
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyProductionCostFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();
		FAssert(pCity != NULL);
		if (pCity != NULL)
		{
			const int iTempValue = pCity->getProductionProgress();
			if (iTempValue > 0)
			{
				if (pCity->getProductionProject() != NO_PROJECT)
				{
					const CvProjectInfo& kProject = GC.getProjectInfo(pCity->getProductionProject());
					iValue += iTempValue * ((kProject.getMaxTeamInstances() == 1) ? 4 : 2);
				}
				else if (pCity->getProductionBuilding() != NO_BUILDING)
				{
					if (isWorldWonder(pCity->getProductionBuilding()))
					{
						iValue += 3 * iTempValue;
					}
					iValue += iTempValue;
				}
				else
				{
					iValue += iTempValue;
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getDestroyUnitCostFactor() > 0)
	{
		/*
		Assassination iValues competes with:
		poisoning (64-768)
		destroy building (2-4439)
		destroy production (8-137)
		revolt (45-150?)
		counter espionage (104-112)
		steal tech (180-17080)
		*/
		SpecialistTypes theGreatSpecialistTarget = (SpecialistTypes)0;

		CvCity* pCity = pPlot->getPlotCity();
		if (NULL != pCity)
		{
			for (int iSpecialist = 7; iSpecialist < GC.getNumSpecialistInfos(); iSpecialist++)
			{
				SpecialistTypes tempSpecialist = (SpecialistTypes)0;
				if (pCity->getFreeSpecialistCount((SpecialistTypes)iSpecialist) > 0)
				{
					tempSpecialist = (SpecialistTypes)iSpecialist;
					if (tempSpecialist > theGreatSpecialistTarget)
					{
						theGreatSpecialistTarget = tempSpecialist;
					}
				}
			}
		}
		if (theGreatSpecialistTarget >= 7)
		{
			iValue += 1000;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getBuyUnitCostFactor() > 0)
	{
		/*
		Bribe iValues compete with:
		destroy improvement (1-60)
		*/
		if (pPlot->plotCount(PUF_isOtherTeam, getID(), -1, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID()) >= 1)
		{
			if (pPlot->plotCount(PUF_isUnitAIType, UNITAI_WORKER, -1) >= 1)
			{
				iValue += 100;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getStealTreasuryTypes() > 0)
	{
		if (pPlot->getPlotCity() != NULL)
		{
			int64_t iGoldStolen = GET_PLAYER(eTargetPlayer).getGold() * GC.getEspionageMissionInfo(eMission).getStealTreasuryTypes() / 100;
			iGoldStolen *= pPlot->getPlotCity()->getPopulation();
			iGoldStolen /= std::max(1, GET_PLAYER(eTargetPlayer).getTotalPopulation());
			iValue += ((GET_PLAYER(eTargetPlayer).AI_isFinancialTrouble() || AI_isFinancialTrouble()) ? 4 : 2) * (2 * std::max<int64_t>(0, iGoldStolen - iCost));
		}
	}

	if (GC.getEspionageMissionInfo(eMission).getCounterespionageNumTurns() > 0)
	{
		//iValue += 100 * GET_TEAM(getTeam()).AI_getAttitudeVal(GET_PLAYER(eTargetPlayer).getTeam());
		// K-Mod (I didn't comment that line out, btw.)
		const TeamTypes eTeam = GET_PLAYER(eTargetPlayer).getTeam();
		const int iEra = getCurrentEra();
		int iCounterValue = 5;
		iCounterValue *= 50 * iEra * iEra + GET_TEAM(eTeam).getEspionagePointsAgainstTeam(getTeam());
		iCounterValue /= std::max(1, 50 * iEra * iEra + GET_TEAM(getTeam()).getEspionagePointsAgainstTeam(eTeam));
		iCounterValue *= AI_getMemoryCount(eTargetPlayer, MEMORY_SPY_CAUGHT) + 1;
		iValue += iCounterValue;

		//TSHEEP - Make Counterespionage matter
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			iValue += std::max((100 - GET_TEAM(getTeam()).AI_getAttitudeVal(GET_PLAYER(eTargetPlayer).getTeam())) * (1 + std::max(AI_getMemoryCount(eTargetPlayer, MEMORY_SPY_CAUGHT), 0)), 0);
		}
		//TSHEEP End
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getBuyCityCostFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			iValue += AI_cityTradeVal(pCity);

			if (GET_PLAYER(pCity->getOwner()).getNumCities() == 1)
			{
				iValue *= 3;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityInsertCultureAmountFactor() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();
		if (NULL != pCity)
		{
			if (pCity->getOwner() != getID())
			{
				int iPlotCulture = pPlot->getCulture(getID());
				int iScale = 1;

				if (iPlotCulture > MAX_INT / 1000)
				{
					iPlotCulture /= 1000;
					iScale = 1000;
				}

				int iCultureAmount = GC.getEspionageMissionInfo(eMission).getCityInsertCultureAmountFactor() * pPlot->getCulture(getID());
				iCultureAmount /= 100;
				iCultureAmount *= iScale;
				if (pCity->calculateCulturePercent(getID()) > 40)
				{
					iValue += iCultureAmount * 3;
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityPoisonWaterCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityHealth = pCity->goodHealth() - pCity->badHealth(false, 0);
			int iBaseUnhealth = GC.getEspionageMissionInfo(eMission).getCityPoisonWaterCounter();

			// K-Mod: fixing some "wtf".
			/*
			int iAvgFoodShortage = std::max(0, iBaseUnhealth - iCityHealth) - pCity->foodDifference();
			iAvgFoodShortage += std::max(0, iBaseUnhealth/2 - iCityHealth) - pCity->foodDifference();

			iAvgFoodShortage /= 2;

			if( iAvgFoodShortage > 0 )
			{
			iValue += 8 * iAvgFoodShortage * iAvgFoodShortage;
			}*/
			int iAvgFoodShortage = std::max(0, iBaseUnhealth - iCityHealth) - pCity->foodDifference();
			iAvgFoodShortage += std::max(0, -iCityHealth) - pCity->foodDifference();

			iAvgFoodShortage /= 2;

			if (iAvgFoodShortage > 0)
			{
				iValue += 4 * iAvgFoodShortage * iBaseUnhealth;
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getCityUnhappinessCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityCurAngerLevel = pCity->happyLevel() - pCity->unhappyLevel(0);
			int iBaseAnger = GC.getEspionageMissionInfo(eMission).getCityUnhappinessCounter();
			int iAvgUnhappy = iCityCurAngerLevel - iBaseAnger / 2;

			if (iAvgUnhappy < 0)
			{
				iValue += 14 * abs(iAvgUnhappy) * iBaseAnger;
			}
		}
	}

	if (GC.getEspionageMissionInfo(eMission).getBuyTechCostFactor() > 0)
	{
		if (iCost < GET_TEAM(getTeam()).getResearchLeft((TechTypes)iData) * 4 / 3)
		{
			int iTempValue = GET_TEAM(getTeam()).AI_techTradeVal((TechTypes)iData, GET_PLAYER(eTargetPlayer).getTeam());

			if (GET_TEAM(getTeam()).getBestKnownTechScorePercent() < 85)
			{
				iTempValue *= 2;
			}

			iValue += iTempValue;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getSwitchCivicCostFactor() > 0)
	{
		iValue += AI_civicTradeVal((CivicTypes)iData, eTargetPlayer);
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getSwitchReligionCostFactor() > 0)
	{
		iValue += AI_religionTradeVal((ReligionTypes)iData, eTargetPlayer);
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getPlayerAnarchyCounter() > 0)
	{
		iValue += GC.getEspionageMissionInfo(eMission).getPlayerAnarchyCounter() * 40;
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isRevolt())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCurRevStatus = pCity->getRevolutionIndex();
			iValue += std::max(300, 300 + (iCurRevStatus / 5));
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isNuke())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iTempValue = 1;

			iTempValue += GC.getGame().getSorenRandNum((pCity->getPopulation() + 1), "AI Nuke City Value");
			iTempValue += std::max(0, pCity->getPopulation() - 10);

			iTempValue += ((pCity->getPopulation() * (100 + pCity->calculateCulturePercent(pCity->getOwner()))) / 100);

			iTempValue += AI_getAttitudeVal(pCity->getOwner()) / 3;

			foreach_(const CvPlot * pLoopPlot, pCity->plot()->adjacent())
			{
				if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					iTempValue++;
				}
				if (pLoopPlot->getNonObsoleteBonusType(getTeam()) != NO_BONUS)
				{
					iTempValue++;
				}
			}
			if (!(pCity->isEverOwned(getID())))
			{
				iTempValue *= 3;
				iTempValue /= 2;
			}
			if (!GET_TEAM(pCity->getTeam()).isAVassal())
			{
				iTempValue *= 2;
			}
			if (pCity->plot()->isVisible(getTeam(), false))
			{
				iTempValue += 2 * pCity->plot()->getNumVisibleUnits(getID());
			}
			else
			{
				iTempValue += 6;
			}

			iValue += iTempValue * 10;
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).isDisablePower())
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (pCity)
		{
			foreach_(const BuildingTypes eTypeX, pCity->getHasBuildings())
			{
				if (!pCity->isDisabledBuilding(eTypeX))
				{
					if (GC.getBuildingInfo(eTypeX).isPrereqPower())
					{
						iValue += 20;
					}
					for (int iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
					{
						iValue += GC.getBuildingInfo(eTypeX).getPowerYieldModifier(iJ);
					}
				}
			}
		}
	}

	if (bMalicious && GC.getEspionageMissionInfo(eMission).getWarWearinessCounter() > 0)
	{
		CvCity* pCity = pPlot->getPlotCity();

		if (NULL != pCity)
		{
			int iCityCurAngerLevel = pCity->happyLevel() - pCity->unhappyLevel(0);
			int iBaseAnger = pCity->getWarWearinessPercentAnger();
			iBaseAnger *= (100 + GC.getEspionageMissionInfo(eMission).getWarWearinessCounter());
			iBaseAnger /= 100;
			iBaseAnger -= pCity->getWarWearinessPercentAnger();
			int iAvgUnhappy = iCityCurAngerLevel - iBaseAnger / 2;

			if (iAvgUnhappy < 0)
			{
				iValue += 14 * abs(iAvgUnhappy) * iBaseAnger;
			}
		}
	}

	if (iValue > MAX_INT)
	{
		FErrorMsg("error");
		return MAX_INT;
	}
	return (int)iValue;
}


int CvPlayerAI::AI_getPeaceWeight() const
{
	return m_iPeaceWeight;
}


void CvPlayerAI::AI_setPeaceWeight(int iNewValue)
{
	m_iPeaceWeight = iNewValue;
}

int CvPlayerAI::AI_getEspionageWeight() const
{
	return m_iEspionageWeight;
}

void CvPlayerAI::AI_setEspionageWeight(int iNewValue)
{
	m_iEspionageWeight = iNewValue;
}


int CvPlayerAI::AI_getAttackOddsChange() const
{
	return m_iAttackOddsChange;
}


void CvPlayerAI::AI_setAttackOddsChange(int iNewValue)
{
	m_iAttackOddsChange = iNewValue;
}


int CvPlayerAI::AI_getCivicTimer() const
{
	return m_iCivicTimer;
}


void CvPlayerAI::AI_setCivicTimer(int iNewValue)
{
	m_iCivicTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(AI_getCivicTimer());
}


void CvPlayerAI::AI_changeCivicTimer(int iChange)
{
	AI_setCivicTimer(AI_getCivicTimer() + iChange);
}


int CvPlayerAI::AI_getReligionTimer() const
{
	return m_iReligionTimer;
}


void CvPlayerAI::AI_setReligionTimer(int iNewValue)
{
	m_iReligionTimer = iNewValue;
	FASSERT_NOT_NEGATIVE(AI_getReligionTimer());
}


void CvPlayerAI::AI_changeReligionTimer(int iChange)
{
	AI_setReligionTimer(AI_getReligionTimer() + iChange);
}

int CvPlayerAI::AI_getExtraGoldTarget() const
{
	return m_iExtraGoldTarget;
}

void CvPlayerAI::AI_setExtraGoldTarget(int iNewValue)
{
	m_iExtraGoldTarget = iNewValue;
}

int CvPlayerAI::AI_getNumTrainAIUnits(UnitAITypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	return m_aiNumTrainAIUnits[eIndex];
}


void CvPlayerAI::AI_changeNumTrainAIUnits(UnitAITypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	m_aiNumTrainAIUnits[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getNumTrainAIUnits(eIndex));
}


int CvPlayerAI::AI_getNumAIUnits(UnitAITypes eIndex) const
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	return m_aiNumAIUnits[eIndex];
}


void CvPlayerAI::AI_changeNumAIUnits(UnitAITypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eIndex);
	m_aiNumAIUnits[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getNumAIUnits(eIndex));
}


int CvPlayerAI::AI_getSameReligionCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiSameReligionCounter[eIndex];
}


void CvPlayerAI::AI_changeSameReligionCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiSameReligionCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getSameReligionCounter(eIndex));
}


int CvPlayerAI::AI_getDifferentReligionCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiDifferentReligionCounter[eIndex];
}


void CvPlayerAI::AI_changeDifferentReligionCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiDifferentReligionCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getDifferentReligionCounter(eIndex));
}


int CvPlayerAI::AI_getFavoriteCivicCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiFavoriteCivicCounter[eIndex];
}


void CvPlayerAI::AI_changeFavoriteCivicCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiFavoriteCivicCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getFavoriteCivicCounter(eIndex));
}


int CvPlayerAI::AI_getBonusTradeCounter(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiBonusTradeCounter[eIndex];
}


void CvPlayerAI::AI_changeBonusTradeCounter(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiBonusTradeCounter[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getBonusTradeCounter(eIndex));
}


int CvPlayerAI::AI_getPeacetimeTradeValue(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiPeacetimeTradeValue[eIndex];
}


void CvPlayerAI::AI_changePeacetimeTradeValue(PlayerTypes eIndex, int iChange)
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);

	if (iChange != 0)
	{
		AI_invalidateAttitudeCache(eIndex);
		GET_PLAYER(eIndex).AI_invalidateAttitudeCache(getID());

		m_aiPeacetimeTradeValue[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(AI_getPeacetimeTradeValue(eIndex));

		if (iChange < 0)
		{
			FErrorMsg("iChange is less than zero");
			return;
		}
		const TeamTypes eTeamB = GET_PLAYER(eIndex).getTeam();

		if (eTeamB != getTeam())
		{
			for (int iPlayerC = 0; iPlayerC < MAX_PC_TEAMS; iPlayerC++)
			{
				CvTeamAI& teamC = GET_TEAM((TeamTypes)iPlayerC);
				/*
				If A trades with B and A is C's worst enemy, C is only mad at B if C has met B before
					A = this
					B = eIndex
					C = iPlayerC
				*/
				if (teamC.isAlive() && teamC.isHasMet(eTeamB) && teamC.AI_getWorstEnemy() == getTeam())
				{
					teamC.AI_changeEnemyPeacetimeTradeValue(eTeamB, iChange);
				}
			}
		}
	}
}


int CvPlayerAI::AI_getPeacetimeGrantValue(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiPeacetimeGrantValue[eIndex];
}


void CvPlayerAI::AI_changePeacetimeGrantValue(PlayerTypes eIndex, int iChange)
{
	PROFILE_FUNC();
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);

	if (iChange != 0)
	{
		m_aiPeacetimeGrantValue[eIndex] += iChange;
		FASSERT_NOT_NEGATIVE(AI_getPeacetimeGrantValue(eIndex));

		if (iChange < 0)
		{
			FErrorMsg("iChange is less than zero");
			return;
		}
		const TeamTypes eTeamB = GET_PLAYER(eIndex).getTeam();

		if (eTeamB != getTeam())
		{
			for (int iPlayerC = 0; iPlayerC < MAX_PC_TEAMS; iPlayerC++)
			{
				CvTeamAI& teamC = GET_TEAM((TeamTypes)iPlayerC);
				/*
				If A trades with B and A is C's worst enemy, C is only mad at B if C has met B before
					A = this
					B = eIndex
					C = iPlayerC
				*/
				if (teamC.isAlive() && teamC.isHasMet(eTeamB) && teamC.AI_getWorstEnemy() == getTeam())
				{
					teamC.AI_changeEnemyPeacetimeGrantValue(eTeamB, iChange);
				}
			}
		}
	}
}


int CvPlayerAI::AI_getGoldTradedTo(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_aiGoldTradedTo[eIndex];
}


void CvPlayerAI::AI_changeGoldTradedTo(PlayerTypes eIndex, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_aiGoldTradedTo[eIndex] += iChange;
	FASSERT_NOT_NEGATIVE(AI_getGoldTradedTo(eIndex));
}


int CvPlayerAI::AI_getAttitudeExtra(const PlayerTypes ePlayer) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);
	return m_aiAttitudeExtra[ePlayer];
}


void CvPlayerAI::AI_setAttitudeExtra(const PlayerTypes ePlayer, const int iNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, ePlayer);

	if (m_aiAttitudeExtra[ePlayer] != iNewValue)
	{
		AI_changeAttitudeCache(ePlayer, iNewValue - m_aiAttitudeExtra[ePlayer]);
		m_aiAttitudeExtra[ePlayer] = iNewValue;
	}
}


void CvPlayerAI::AI_changeAttitudeExtra(const PlayerTypes ePlayer, const int iChange)
{
	AI_setAttitudeExtra(ePlayer, (AI_getAttitudeExtra(ePlayer) + iChange));
}


bool CvPlayerAI::AI_isFirstContact(PlayerTypes eIndex) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	return m_abFirstContact[eIndex];
}


void CvPlayerAI::AI_setFirstContact(PlayerTypes eIndex, bool bNewValue)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex);
	m_abFirstContact[eIndex] = bNewValue;
}


int CvPlayerAI::AI_getContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eIndex2);
	return m_aaiContactTimer[eIndex1][eIndex2];
}


void CvPlayerAI::AI_changeContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_CONTACT_TYPES, eIndex2);

	if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS) && eIndex1 != NO_PLAYER && !GET_PLAYER(eIndex1).isHumanPlayer() && iChange > 0)
	{
		// Afforess - AI's trade with AI's much more often
		m_aaiContactTimer[eIndex1][eIndex2] += (iChange / 3);
	}
	else
	{
		m_aaiContactTimer[eIndex1][eIndex2] += iChange;
	}
	FASSERT_NOT_NEGATIVE(m_aaiContactTimer[eIndex1][eIndex2]);
}


int CvPlayerAI::AI_getMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2) const
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eIndex2);
	return m_aaiMemoryCount[eIndex1][eIndex2];
}


void CvPlayerAI::AI_changeMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2, int iChange)
{
	FASSERT_BOUNDS(0, MAX_PLAYERS, eIndex1);
	FASSERT_BOUNDS(0, NUM_MEMORY_TYPES, eIndex2);

	m_aaiMemoryCount[eIndex1][eIndex2] += iChange;

	if (eIndex1 == GC.getGame().getActivePlayer())
	{
		// BUG - Update Attitude Icons
		gDLL->getInterfaceIFace()->setDirty(Score_DIRTY_BIT, true);
	}
	FASSERT_NOT_NEGATIVE(AI_getMemoryCount(eIndex1, eIndex2));
}

int CvPlayerAI::AI_calculateGoldenAgeValue() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;
	for (int iI = 0; iI < NUM_YIELD_TYPES; ++iI)
	{
		iValue += (
			GC.getYieldInfo((YieldTypes)iI).getGoldenAgeYield() * AI_yieldWeight((YieldTypes)iI)
			/ std::max(1, 1 + GC.getYieldInfo((YieldTypes)iI).getGoldenAgeYieldThreshold())
		);
	}
	iValue *= getTotalPopulation() * getGoldenAgeLength();
	iValue /= 100;

	// Golden Ages Reduce Revolutions
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
	{
		int iNationalRevIndex = 0;
		foreach_(const CvCity * pLoopCity, cities())
		{
			iNationalRevIndex += pLoopCity->getRevolutionIndex();
		}
		iNationalRevIndex /= std::max(1, getNumCities());

		iValue *= std::max(1, iNationalRevIndex / 500);
	}
	return iValue;
}

// Protected Functions...

void CvPlayerAI::AI_doCounter()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAliveAndTeam(getTeam(), false)
		&& GET_TEAM(getTeam()).isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
		{
			if (getStateReligion() != NO_RELIGION
			&&  getStateReligion() == GET_PLAYER((PlayerTypes)iI).getStateReligion())
			{
				AI_changeSameReligionCounter((PlayerTypes)iI, 1);
			}
			else if (AI_getSameReligionCounter((PlayerTypes)iI) > 0)
			{
				AI_changeSameReligionCounter((PlayerTypes)iI, -1);
			}

			if (getStateReligion() != NO_RELIGION
			&&  getStateReligion() != GET_PLAYER((PlayerTypes)iI).getStateReligion()
			&& GET_PLAYER((PlayerTypes)iI).getStateReligion() != NO_RELIGION)
			{
				AI_changeDifferentReligionCounter((PlayerTypes)iI, 1);
			}
			else if (AI_getDifferentReligionCounter((PlayerTypes)iI) > 0)
			{
				AI_changeDifferentReligionCounter((PlayerTypes)iI, -1);
			}

			if (GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic() != NO_CIVIC)
			{
				if (isCivic((CivicTypes) GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic())
				&& GET_PLAYER((PlayerTypes)iI).isCivic((CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic()))
				{
					AI_changeFavoriteCivicCounter(((PlayerTypes)iI), 1);
				}
				else if (AI_getFavoriteCivicCounter((PlayerTypes)iI) > 0)
				{
					AI_changeFavoriteCivicCounter(((PlayerTypes)iI), -1);
				}
			}

			const int iBonusImports = getNumTradeBonusImports((PlayerTypes)iI);

			if (iBonusImports > 0)
			{
				AI_changeBonusTradeCounter(((PlayerTypes)iI), iBonusImports);
			}
			else
			{
				AI_changeBonusTradeCounter(((PlayerTypes)iI), -(std::min(AI_getBonusTradeCounter((PlayerTypes)iI), ((GET_PLAYER((PlayerTypes)iI).getNumCities() / 4) + 1))));
			}
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			for (int iJ = 0; iJ < NUM_CONTACT_TYPES; iJ++)
			{
				if (AI_getContactTimer(((PlayerTypes)iI), ((ContactTypes)iJ)) > 0)
				{
					AI_changeContactTimer(((PlayerTypes)iI), ((ContactTypes)iJ), -1);
				}
			}
		}
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
			{
				if (AI_getMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ) > 0
				&& GC.getLeaderHeadInfo(getPersonalityType()).getMemoryDecayRand(iJ) > 0)
				{
					// Afforess - 04/26/14 - Ruthless AI: Easier for the AI to forget past wrongs
					//	AI attitude is designed to make AI players feel human, but it makes them weak
					//	A Perfect AI treats enemies and friends alike, both are obstacles to victory
					int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getMemoryDecayRand(iJ);

					if (GC.getGame().isModderGameOption(MODDERGAMEOPTION_REALISTIC_DIPLOMACY))
					{
						iRand /= 1 + AI_getMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ);
					}

					if (GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
					{
						iRand /= 3;
					}

					if (GC.getGame().getSorenRandNum(iRand, "Memory Decay") == 0)
					{
						AI_changeMemoryCount((PlayerTypes)iI, (MemoryTypes)iJ, -1);
					}
				}
			}
		}
	}
}

void CvPlayerAI::AI_doMilitary()
{
	PROFILE_FUNC();

	// Afforess - add multiple passes
	if (AI_isFinancialTrouble() && !GET_TEAM(getTeam()).hasWarPlan(true))
	{
		const short iSafePercent = AI_safeFunding();
		int64_t iNetIncome = getCommerceRate(COMMERCE_GOLD) + std::max(0, getGoldPerTurn());
		int64_t iNetExpenses;

		for (int iPass = 0; iPass < 4; iPass++)
		{
			short iProfitMargin = getProfitMargin(iNetExpenses);

			while (iNetIncome < iNetExpenses && iProfitMargin < iSafePercent && getUnitUpkeepMilitaryNet() > 0)
			{
				int iExperienceThreshold;
				switch (iPass)
				{
				case 0: iExperienceThreshold = 1; break;
				case 1: iExperienceThreshold = 6; break;
				case 2: iExperienceThreshold = 12; break;
				case 3: iExperienceThreshold = -1; break;
				}
				if (!AI_disbandUnit(iExperienceThreshold))
				{
					break;
				}
				// Recalculate funding
				iProfitMargin = getProfitMargin(iNetExpenses);
				iNetIncome = getCommerceRate(COMMERCE_GOLD) + std::max(0, getGoldPerTurn());
			}
		}
	}
	AI_setAttackOddsChange(
		  GC.getLeaderHeadInfo(getPersonalityType()).getBaseAttackOddsChange()
		+ GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getAttackOddsChangeRand(), "AI Attack Odds Change #1")
		+ GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getAttackOddsChangeRand(), "AI Attack Odds Change #2")
	);
}


void CvPlayerAI::AI_doCommerce()
{
	PROFILE_FUNC();

	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	if (isNPC() || getAnarchyTurns() > 0)
	{
		return;
	}
	int64_t iGoldTarget = AI_goldTarget();

	const bool bFlexResearch = isCommerceFlexible(COMMERCE_RESEARCH);
	const bool bFlexCulture = isCommerceFlexible(COMMERCE_CULTURE);
	const bool bFlexEspionage = isCommerceFlexible(COMMERCE_ESPIONAGE);

	const TechTypes eCurrentResearch = getCurrentResearch();
	if (bFlexResearch && eCurrentResearch != NO_TECH && !AI_avoidScience())
	{
		// Set research rate to 100%
		setCommercePercent(COMMERCE_RESEARCH, 100);

		// If we can finish the current research without spending a third of our gold, lower gold target by a third.
		const int iGoldRate = calculateGoldRate();
		if (iGoldRate < 0 && getGold() / 3 >= getResearchTurnsLeft(eCurrentResearch, true) * iGoldRate)
		{
			iGoldTarget *= 2;
			iGoldTarget /= 3;
		}
	}

	bool bReset = false;

	if (bFlexCulture && getCommercePercent(COMMERCE_CULTURE) > 0)
	{
		setCommercePercent(COMMERCE_CULTURE, 0);
		bReset = true;
	}

	if (bFlexEspionage)
	{
		// Reset espionage spending always
		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; ++iTeam)
		{
			setEspionageSpendingWeightAgainstTeam((TeamTypes)iTeam, 0);
		}
		if (getCommercePercent(COMMERCE_ESPIONAGE) > 0)
		{
			setCommercePercent(COMMERCE_ESPIONAGE, 0);
			bReset = true;
		}
	}

	if (bReset)
	{
		AI_assignWorkingPlots();
	}
	const int iIncrement = std::max(1, GC.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"));

	if (bFlexCulture && getNumCities() > 0)
	{
		int iIdealPercent = 0;

		if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
		{
			iIdealPercent = 100;
		}
		else
		{
			foreach_(const CvCity * pLoopCity, cities())
			{
				if (pLoopCity->getCommerceHappinessPer(COMMERCE_CULTURE) > 0)
				{
					iIdealPercent += pLoopCity->angryPopulation() * 100 / pLoopCity->getCommerceHappinessPer(COMMERCE_CULTURE);
				}
			}
			iIdealPercent /= getNumCities();

			iIdealPercent -= iIdealPercent % iIncrement;

			iIdealPercent = std::min(iIdealPercent, 20);
		}
		setCommercePercent(COMMERCE_CULTURE, iIdealPercent);
	}
	const bool bFirstTech = AI_isFirstTech(eCurrentResearch);
	const int iTargetTurns = GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / 10;
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);

	if (bFlexResearch)
	{
		if (!bFirstTech && (isNoResearchAvailable() || AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4)))
		{
			setCommercePercent(COMMERCE_RESEARCH, 0);
		}
		else if (!bFirstTech)
		{
			if (AI_avoidScience())
			{
				changeCommercePercent(COMMERCE_RESEARCH, -10);
			}
			if (team.getChosenWarCount(true) > 0 || team.getWarPlanCount(WARPLAN_ATTACKED_RECENT, true) > 0)
			{
				changeCommercePercent(COMMERCE_RESEARCH, -5);
			}
			const int iOldPercent = getCommercePercent(COMMERCE_RESEARCH);
			const int iOldGoldRate = getCommerceRate(COMMERCE_GOLD);
			const int iOldBeakerRate = getCommerceRate(COMMERCE_RESEARCH);

			int iInc = iIncrement;
			int iCount = 0;
			int iGoldRate = calculateGoldRate();
			while (getGold() + iTargetTurns * iGoldRate < iGoldTarget)
			{
				if ((bFirstTech || ++iCount > 3 && getCommercePercent(COMMERCE_RESEARCH) < 50) && iGoldRate > 0)
				{
					break; // Don't sacrifice too much science to reach gold target.
				}
				const int iPrevGoldRate = getCommerceRate(COMMERCE_GOLD);
				changeCommercePercent(COMMERCE_RESEARCH, -iInc);
				if (iPrevGoldRate == getCommerceRate(COMMERCE_GOLD))
				{
					changeCommercePercent(COMMERCE_RESEARCH, iInc);
					if (getCommercePercent(COMMERCE_RESEARCH) == iInc)
					{
						break;
					}
					iInc += iIncrement;
				}
				else if (getCommercePercent(COMMERCE_RESEARCH) == 0)
				{
					if (calculateGoldRate() >= 0)
					{
						setCommercePercent(COMMERCE_RESEARCH, iIncrement);
						if (calculateGoldRate() < 1)
						{
							setCommercePercent(COMMERCE_RESEARCH, 0);
						}
					}
					break;
				}
				iGoldRate = calculateGoldRate();
			}
			const int iNewPercent = getCommercePercent(COMMERCE_RESEARCH);
			if (iNewPercent < iOldPercent)
			{
				const int iBeakerLoss = iOldBeakerRate - getCommerceRate(COMMERCE_RESEARCH);
				const int iGoldGain = getCommerceRate(COMMERCE_GOLD) - iOldGoldRate;
				if (
					iBeakerLoss > iGoldGain
				&&
					(
						// 5 % more tax doesn't even give 1 gold (typical early prehistoric)
						iOldPercent - iNewPercent >= 5 * iGoldGain
						// or tradeoff is not close to comparably worth it.
						|| iBeakerLoss > 3 * iGoldGain && !AI_isFinancialTrouble()
						)
				) setCommercePercent(COMMERCE_RESEARCH, iOldPercent);
			}
		}
	}

	if (bFlexEspionage && !bFirstTech)
	{
		int iEspionageTargetRate = 0;
		int* piTarget = new int[MAX_PC_TEAMS];
		int* piWeight = new int[MAX_PC_TEAMS];

		for (int iTeamX = 0; iTeamX < MAX_PC_TEAMS; ++iTeamX)
		{
			piTarget[iTeamX] = 0;
			piWeight[iTeamX] = 0;

			const TeamTypes eTeamX = (TeamTypes)iTeamX;
			if (eTeamX == eTeam || !team.isHasMet(eTeamX) || team.isVassal(eTeamX))
				continue;

			const CvTeam& teamX = GET_TEAM(eTeamX);
			if (teamX.isAlive() && !teamX.isVassal(eTeam))
			{
				int iTheirEspPoints = teamX.getEspionagePointsAgainstTeam(eTeam);
				int iDesiredMissionPoints = 0;

				piWeight[iTeamX] = 10;
				int iRateDivisor = 12;

				if (team.AI_getWarPlan(eTeamX) != NO_WARPLAN)
				{
					iTheirEspPoints *= 3;
					iTheirEspPoints /= 2;

					for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
					{
						const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

						if (kMissionInfo.isPassive() && (kMissionInfo.isSeeDemographics() || kMissionInfo.isSeeResearch()))
						{
							const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
							if (iDesiredMissionPoints < iMissionCost)
							{
								iDesiredMissionPoints = iMissionCost;
							}
						}
					}

					iRateDivisor = 10;
					piWeight[iTeamX] = 20;

					if (team.AI_hasCitiesInPrimaryArea(eTeamX))
					{
						piWeight[iTeamX] = 30;
						iRateDivisor = 8;
					}
				}
				else
				{
					const int iAttitude = range(team.AI_getAttitudeVal(eTeamX), -12, 12);

					iTheirEspPoints -= iTheirEspPoints * iAttitude / 24;

					if (iAttitude <= -3)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.isPassive() && (kMissionInfo.isSeeDemographics() || kMissionInfo.isSeeResearch()))
							{
								const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
								if (iDesiredMissionPoints < iMissionCost)
								{
									iDesiredMissionPoints = iMissionCost;
								}
							}
						}
					}
					else if (iAttitude < 3)
					{
						for (int iMission = 0; iMission < GC.getNumEspionageMissionInfos(); ++iMission)
						{
							const CvEspionageMissionInfo& kMissionInfo = GC.getEspionageMissionInfo((EspionageMissionTypes)iMission);

							if (kMissionInfo.isPassive() && kMissionInfo.isSeeDemographics())
							{
								const int iMissionCost = getEspionageMissionCost((EspionageMissionTypes)iMission, teamX.getLeaderID(), NULL, -1, NULL) * 11 / 10;
								if (iDesiredMissionPoints < iMissionCost)
								{
									iDesiredMissionPoints = iMissionCost;
								}
							}
						}
					}
					iRateDivisor += iAttitude / 5;
					piWeight[iTeamX] -= iAttitude / 2;
				}

				const int iDesiredEspPoints = std::max(iTheirEspPoints, iDesiredMissionPoints);
				const int iOurEspPoints = team.getEspionagePointsAgainstTeam(eTeamX);

				piTarget[iTeamX] = (iDesiredEspPoints - iOurEspPoints) / std::max(6, iRateDivisor);

				if (piTarget[iTeamX] > 0)
				{
					iEspionageTargetRate += piTarget[iTeamX];
				}
			}
		}

		for (int iI = 0; iI < MAX_PC_TEAMS; ++iI)
		{
			if (piTarget[iI] > 0)
			{
				piWeight[iI] += (150 * piTarget[iI]) / std::max(4, iEspionageTargetRate);
			}
			else if (piTarget[iI] < 0)
			{
				piWeight[iI] += 2 * piTarget[iI];
			}
			setEspionageSpendingWeightAgainstTeam((TeamTypes)iI, std::max(0, piWeight[iI]));
		}
		SAFE_DELETE_ARRAY(piTarget);
		SAFE_DELETE_ARRAY(piWeight);

		// If economy is weak, neglect espionage spending. Invest hammers into espionage via spies/builds instead.
		if (AI_isFinancialTrouble() || AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
		{
			iEspionageTargetRate = 0;
		}
		else
		{
			iEspionageTargetRate *= (110 - getCommercePercent(COMMERCE_GOLD) * 2);
			iEspionageTargetRate /= 110;

			iEspionageTargetRate *= GC.getLeaderHeadInfo(getLeaderType()).getEspionageWeight();
			iEspionageTargetRate /= 100;

			int iMaxEspionage = AI_isFinancialTrouble() ? 0 : 5;

			if (iMaxEspionage > 0 && team.getChosenWarCount(true) == 0 && team.getWarPlanCount(WARPLAN_ATTACKED_RECENT, true) == 0)
			{
				const int iRank = GC.getGame().getPlayerRank(getID());

				if (iRank < 2)
				{
					iMaxEspionage = 15;
				}
				else if (iRank < 4)
				{
					iMaxEspionage = 10;
				}
			}

			while (getCommerceRate(COMMERCE_ESPIONAGE) < iEspionageTargetRate && getCommercePercent(COMMERCE_ESPIONAGE) < iMaxEspionage)
			{
				changeCommercePercent(COMMERCE_RESEARCH, -iIncrement);
				changeCommercePercent(COMMERCE_ESPIONAGE, iIncrement);

				if (getGold() + iTargetTurns * calculateGoldRate() < iGoldTarget)
				{
					break;
				}
				if (!AI_avoidScience() && !isNoResearchAvailable()
				// Keep Espionage percent somewhat lower than research percent
				&& getCommercePercent(COMMERCE_RESEARCH) * 2 <= 3 * (getCommercePercent(COMMERCE_ESPIONAGE) + iIncrement))
				{
					break;
				}
			}
		}
	}

	if (!bFirstTech && getGold() < iGoldTarget && getCommercePercent(COMMERCE_RESEARCH) > 40)
	{
		for (int iHurry = 0; iHurry < GC.getNumHurryInfos(); iHurry++)
		{
			if (GC.getHurryInfo((HurryTypes)iHurry).getGoldPerProduction() > 0 && canHurry((HurryTypes)iHurry))
			{
				(
					(getCommercePercent(COMMERCE_ESPIONAGE) > 0)
					?
					changeCommercePercent(COMMERCE_ESPIONAGE, -iIncrement)
					:
					changeCommercePercent(COMMERCE_RESEARCH, -iIncrement)
				);
				break;
			}
		}
	}
	// this is called on doTurn, so make sure our gold is high enough keep us above zero gold.
	verifyGoldCommercePercent();
}


void CvPlayerAI::AI_doCivics()
{
	PROFILE_FUNC();
	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");


	m_turnsSinceLastRevolution++;
	m_iCivicSwitchMinDeltaThreshold = (m_iCivicSwitchMinDeltaThreshold * 95) / 100;

	if (isNPC())
	{
		return;
	}

	if (AI_getCivicTimer() > 0)
	{
		AI_changeCivicTimer(-1);
		return;
	}

	if (!canRevolution(NULL))
	{
		return;
	}
	CivicTypes* paeBestCivic = NULL;
	int* paeBestCivicValue = NULL;
	int* paeBestNearFutureCivicValue = NULL;
	int* paeCurCivicValue = NULL;
	int* paiAvailableChoices = NULL;
	int iCurCivicsValue = 0;
	int iBestCivicsValue = 0;

	m_iCityGrowthValueBase = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		int iCityHappy = pLoopCity->happyLevel() - pLoopCity->unhappyLevel();
		int iCurrentFoodToGrow = pLoopCity->growthThreshold();
		int iFoodPerTurn = pLoopCity->foodDifference(false, true, true);
		int iCityValue = 0;

		if (gPlayerLogLevel > 1)
		{
			logBBAI("Player %d (%S) city %S has food/turn %d (%d from trade), happy %d",
					getID(),
					getCivilizationDescription(0),
					pLoopCity->getName().c_str(),
					iFoodPerTurn,
					pLoopCity->getTradeYield(YIELD_FOOD),
					iCityHappy);
		}

		iCityValue = iCurrentFoodToGrow;

		int iFoodDiffDivisor = std::abs(iFoodPerTurn - 1) + 5;
		int iHappyDivisor = std::max(1, -iCityHappy + 1) + 4;

//#if 0
//		//	We Always count at least 3 food per turn on any city we evaluate at all, and want to
//		//	evaluate any that are near suplus.  This is to promote civic stability, since small
//		//	health changes are likely in any civic switch and we don't want them to move a city
//		//	from not counting at all to counting a lot
//		if (iFoodPerTurn > -2)
//		{
//			if (iCityHappy >= 0)
//			{
//				//	We look at the food difference without trade yields because otherwise civic switches that change the trade
//				//	yield can wildly distort the value of growth.
//				iCityValue = (std::min(3, iCityHappy + 1) * iCurrentFoodToGrow) / std::max(3, iFoodPerTurn - pLoopCity->getTradeYield(YIELD_FOOD));
//				if (gPlayerLogLevel > 1)
//				{
//					logBBAI("Player %d (%S) city %S growth value %d",
//							getID(),
//							getCivilizationDescription(0),
//							pLoopCity->getName().c_str(),
//							iCityValue);
//				}
//			}
//		}
//#else
		iCityValue = (iCityValue * 10) / (iFoodDiffDivisor + iHappyDivisor);
		if (gPlayerLogLevel > 1)
		{
			logBBAI("Player %d (%S) city %S growth value %d",
					getID(),
					getCivilizationDescription(0),
					pLoopCity->getName().c_str(),
					iCityValue);
		}
//#endif

		m_iCityGrowthValueBase += iCityValue;
	}

	FAssertMsg(AI_getCivicTimer() == 0, "AI Civic timer is expected to be 0");

	paeBestCivic = new CivicTypes[GC.getNumCivicOptionInfos()];
	paeBestCivicValue = new int[GC.getNumCivicOptionInfos()];
	paeCurCivicValue = new int[GC.getNumCivicOptionInfos()];
	paeBestNearFutureCivicValue = new int[GC.getNumCivicOptionInfos()];
	paiAvailableChoices = new int[GC.getNumCivicOptionInfos()];

	bool bDoRevolution = false;
	int iCurValue;
	int iBestValue;

	for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
	{
		m_aiCivicValueCache[iI] = MAX_INT;
	}

	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		paiAvailableChoices[iI] = 0;
	}

	for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
	{
		if (canDoCivics((CivicTypes)iI))
		{
			const int iCivicOption = GC.getCivicInfo((CivicTypes)iI).getCivicOptionType();

			FASSERT_BOUNDS(0, GC.getNumCivicOptionInfos(), iCivicOption);

			paiAvailableChoices[iCivicOption]++;
		}
	}
	/*
		//Might be good to have if many civics from different cathegories affect each other much and become available at the same time
		// otherwise this is not needed and therefore commented out
		//To use this, simply uncomment and replace any "iI" or "(CivicOptionTypes)iI" in the rest of this this function
		// with "(paeShuffledCivicOptions[iI])"
		CivicOptionTypes* paeShuffledCivicOptions = new Array[GC.getNumCivicOptionInfos()];
		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			paeShuffledCivicOptions[iI] = (CivicOptionTypes)iI;
		}
		int iNumPermutations = 1;
		for (int iI = GC.getNumCivicOptionInfos(); iI > 1; iI--)
		{
			iNumPermutations *= iI;
		}
		int iPermutation = GC.getGame().getSorenRandNum(iNumPermutations, "AI Civic Option Shuffling");
		//mapping each possible iPermutation to one possible permutation
		int iPermutationWidth;
		CivicOptionTypes eTempShuffleCivicOption;
		for (int iI = GC.getNumCivicOptionInfos(); (iI > 0 && iPermutation > 0); iI--)
		{
			iPermutationWidth = iPermutation % iI;
			iNumPermutations /= iI;
			iPermutation %= iNumPermutations;
			if (iPermutationWidth > 0)
			{
				eTempShuffleCivicOption = paeShuffledCivicOptions[iI];
				paeShuffledCivicOptions[iI] = paeShuffledCivicOptions[(iI+iPermutationWidth)];
				paeShuffledCivicOptions[(iI+iPermutationWidth)] = eTempShuffleCivicOption;
			}
		}
		SAFE_DELETE_ARRAY(paeShuffledCivicOptions); //<- not to be forgotten at the end of this function
	*/

	//initializing
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
	}


	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (paiAvailableChoices[iI] > 1)
		{
			//civic option vacuum
			if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
				processCivics(getCivics((CivicOptionTypes)iI), -1, /* bLimited */ true);

			paeBestCivic[iI] = AI_bestCivic((CivicOptionTypes)iI, &iBestValue, /* bCivicOptionVacuum */ true, paeBestCivic);
			paeCurCivicValue[iI] = AI_civicValue(getCivics((CivicOptionTypes)iI), /* bCivicOptionVacuum */ true, paeBestCivic);

			if (paeBestCivic[iI] == NO_CIVIC || iBestValue <= paeCurCivicValue[iI])
			{
				paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
				paeBestCivicValue[iI] = paeCurCivicValue[iI];
			}
			else
			{
				FAssert(paeBestCivic[iI] != 0);
				paeBestCivicValue[iI] = iBestValue;
			}

			if (paeBestCivic[iI] != NO_CIVIC && paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
			{
				logBBAI("Player %d (%S) suggests civic switch from %S->%S on initial pass (values %d and %d)",
						getID(),
						getCivilizationDescription(0),
						GC.getCivicInfo(getCivics((CivicOptionTypes)iI)).getDescription(),
						GC.getCivicInfo(paeBestCivic[iI]).getDescription(),
						paeCurCivicValue[iI],
						paeBestCivicValue[iI]);
				bDoRevolution = true;
			}

			if (paeBestCivic[iI] != NO_CIVIC)
				processCivics(paeBestCivic[iI], 1, /* bLimited */ true);
		}
		else
		{
			paeBestCivicValue[iI] = -1;	//	Not set
		}
	}

	//repeat? just to be sure we aren't doing anything stupid
	bool bChange = (bDoRevolution);
	int iPass = 0;
	while (bChange && iPass < GC.getNumCivicOptionInfos())
	{
		for (int iI = 0; iI < GC.getNumCivicInfos() * 2; iI++)
		{
			m_aiCivicValueCache[iI] = MAX_INT;
		}

		iPass++;
		FAssertMsg(iPass <= 2, "Civic decision takes too long.");
		bChange = false;
		iBestCivicsValue = 0;
		iCurCivicsValue = 0;

		CivicTypes eNewBestCivic;

		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			if (paiAvailableChoices[iI] > 1)
			{
				if (paeBestCivic[iI] != NO_CIVIC)
					processCivics(paeBestCivic[iI], -1, /* bLimited */ true);

				eNewBestCivic = AI_bestCivic((CivicOptionTypes)iI, &iBestValue, /* bCivicOptionVacuum */ true, paeBestCivic);

				iCurValue = AI_civicValue(getCivics((CivicOptionTypes)iI), /* bCivicOptionVacuum */ true, paeBestCivic);

				if (eNewBestCivic == NO_CIVIC || iBestValue < iCurValue)
				{
					if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
					{
						bChange = true;
						paeBestCivic[iI] = getCivics((CivicOptionTypes)iI);
						if (eNewBestCivic == NO_CIVIC) //when does this happen?
						{
							paeBestCivicValue[iI] = iCurValue;
							iBestValue = iCurValue;
						}

						logBBAI("Player %d (%S) prefers original civic %S on pass %d (values %d and %d)",
								getID(),
								getCivilizationDescription(0),
								GC.getCivicInfo(paeBestCivic[iI]).getDescription(),
								iPass,
								paeCurCivicValue[iI],
								paeBestCivicValue[iI]);
					}
				}
				else
				{
					if (paeBestCivic[iI] != eNewBestCivic)
					{
						bChange = true;
						paeBestCivic[iI] = eNewBestCivic;
						paeBestCivicValue[iI] = iBestValue;

						logBBAI("Player %d (%S) prefers civic switch from %S->%S on pass %d (values %d and %d)",
								getID(),
								getCivilizationDescription(0),
								GC.getCivicInfo(getCivics((CivicOptionTypes)iI)).getDescription(),
								GC.getCivicInfo(paeBestCivic[iI]).getDescription(),
								iPass,
								paeCurCivicValue[iI],
								paeBestCivicValue[iI]);
					}
				}
				iBestCivicsValue += iBestValue;
				iCurCivicsValue += iCurValue;

				if (paeBestCivic[iI] != NO_CIVIC)
					processCivics(paeBestCivic[iI], 1, /* bLimited */ true);
			}
		}
	}

	//	Put back current civics
	int iCivicChanges = 0;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (paiAvailableChoices[iI] > 1)
		{
			if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
			{
				if (paeBestCivic[iI] != NO_CIVIC)
				{
					iCivicChanges++;
					processCivics(paeBestCivic[iI], -1, /* bLimited */ true);

					if (gPlayerLogLevel > 1)
					{
						logBBAI("Player %d (%S) considering civic switch from %S->%S (values %d and %d)",
								getID(),
								getCivilizationDescription(0),
								GC.getCivicInfo(getCivics((CivicOptionTypes)iI)).getDescription(),
								GC.getCivicInfo(paeBestCivic[iI]).getDescription(),
								paeCurCivicValue[iI],
								paeBestCivicValue[iI]);
					}
				}

				if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
					processCivics(getCivics((CivicOptionTypes)iI), 1, /* bLimited */ true);
			}
		}
	}

	if (bDoRevolution)
	{
		FAssert(iBestCivicsValue >= iCurCivicsValue);

		// If we have no anarchy or if we're in a golden age that will still be going
		//	next time we get an opportunity to change civs just do it (it's costless)
		if (getMaxAnarchyTurns() != 0
		&& (!isGoldenAge() || GC.getDefineINT("MIN_REVOLUTION_TURNS") >= getGoldenAgeTurns()))
		{
			if (iBestCivicsValue - iCurCivicsValue < m_iCivicSwitchMinDeltaThreshold)
			{
				if (gPlayerLogLevel > 1)
				{
					logBBAI("Civic switch deferred since minimum value threshold not reached (%d vs %d)",
							iBestCivicsValue - iCurCivicsValue,
							m_iCivicSwitchMinDeltaThreshold);
				}
				bDoRevolution = false;
			}
			else // Are we close to discovering new civic enablers?  If so should we wait for them?
			{
				bDoRevolution = false;
				while (iCivicChanges > 0 && !bDoRevolution)
				{
					int	iNearFutureCivicsBestValue = iBestCivicsValue;
					int iMaxHorizon = 20 * getCivicAnarchyLength(paeBestCivic);

					for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
					{
						paeBestNearFutureCivicValue[iI] = 0;
					}

					for (int iOptionType = 0; iOptionType < GC.getNumCivicOptionInfos(); iOptionType++)
					{
						bool bTestSwitched = false;

						for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
						{
							const CivicTypes eCivic = static_cast<CivicTypes>(iI);

							if (GC.getCivicInfo(eCivic).getCivicOptionType() != iOptionType || canDoCivics(eCivic))
							{
								continue;
							}
							const TechTypes eTech = GC.getCivicInfo(eCivic).getTechPrereq();

							if (GET_TEAM(getTeam()).isHasTech(eTech) || GC.getTechInfo(eTech).getEra() > getCurrentEra() + 1)
							{
								continue;
							}
							// civic option vacuum
							if (getCivics((CivicOptionTypes)iOptionType) != NO_CIVIC && !bTestSwitched)
							{
								processCivics(getCivics((CivicOptionTypes)iOptionType), -1, /* bLimited */ true);
								bTestSwitched = true;
							}
							const int iNearFutureValue = AI_civicValue(eCivic, /* bCivicOptionVacuum */ true, paeBestCivic);

							if (paeBestCivicValue[iOptionType] == -1)
							{
								// Not calculated yet - do so now
								paeBestCivicValue[iOptionType] = AI_civicValue(paeBestCivic[iOptionType], /* bCivicOptionVacuum */ true, paeBestCivic);
							}

							if (gPlayerLogLevel > 1)
							{
								logBBAI("Near future civic %S has value %d, vs current %d for that civic option",
										GC.getCivicInfo(eCivic).getDescription(),
										iNearFutureValue,
										paeBestCivicValue[iOptionType]);
							}

							if (iNearFutureValue > paeBestCivicValue[iOptionType])
							{
								const int iTurns = std::max(1, findPathLength(eTech, true) / std::max(1, calculateResearchRate()));

								if (gPlayerLogLevel > 1)
								{
									logBBAI("Tech path len is %d vs horizon %d", iTurns, iMaxHorizon);
								}

								if (iTurns <= iMaxHorizon)
								{
									int weightedDelta = (iNearFutureValue - paeBestCivicValue[iOptionType]) * 20 / (20 + iTurns);

									if (gPlayerLogLevel > 1)
									{
										logBBAI("weightedDelta is %d vs best %d", weightedDelta, paeBestNearFutureCivicValue[iOptionType]);
									}

									if (weightedDelta > paeBestNearFutureCivicValue[iOptionType])
									{
										iNearFutureCivicsBestValue -= paeBestNearFutureCivicValue[iOptionType];
										iNearFutureCivicsBestValue += weightedDelta;

										paeBestNearFutureCivicValue[iOptionType] = weightedDelta;

										if (gPlayerLogLevel > 0)
										{
											logBBAI(
												"Found near-future civic %S with value %d",
												GC.getCivicInfo(eCivic).getDescription(), iNearFutureValue
											);
										}
									}
								}
							}
						}

						if (bTestSwitched)
						{
							processCivics(getCivics((CivicOptionTypes)iOptionType), 1, /* bLimited */ true);
						}
					}

					FAssert(iNearFutureCivicsBestValue >= iBestCivicsValue);

					//	So if the best we can do now is an improvement, and the degree of improvement is greater than
					//	the time-discounted degree of additional improvement we'd get from near future civic just do it now
					bDoRevolution = (iBestCivicsValue > iCurCivicsValue && (iBestCivicsValue - iCurCivicsValue) > (iNearFutureCivicsBestValue - iBestCivicsValue));

					if (gPlayerLogLevel > 1)
					{
						logBBAI("Gain from immediate switch is %d, additional gain from near-future switch %d",
								iBestCivicsValue - iCurCivicsValue,
								iNearFutureCivicsBestValue - iBestCivicsValue);
					}

					if (bDoRevolution)
					{
						//	Factor in lost production/GNP due to anarchy
						int iTotalEconomyTurnValue = (getCommerceRate(COMMERCE_GOLD) +
													  getCommerceRate(COMMERCE_RESEARCH) +
													  getCommerceRate(COMMERCE_CULTURE) +
													  getCommerceRate(COMMERCE_ESPIONAGE) +
													  2 * calculateTotalYield(YIELD_PRODUCTION));
						int	iPerTurnEstimatedIncrease = (iBestCivicsValue - iCurCivicsValue);
						int iAnarchyCost = getCivicAnarchyLength(paeBestCivic) * iTotalEconomyTurnValue;
						int iBenfitInTurns = std::min(50, m_turnsSinceLastRevolution) * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent() / 100;
						int iBenefit = iPerTurnEstimatedIncrease * iBenfitInTurns;

						if (gPlayerLogLevel > 0)
						{
							logBBAI("Estimated number of turns to justify %d (threshold for switch, with %d turns since last revolution, at this game speed: %d)",
									iAnarchyCost / iPerTurnEstimatedIncrease,
									m_turnsSinceLastRevolution,
									iBenfitInTurns);
						}

						//	If we won't make it up in (arbitrary number) 50 turns (at standard speed) don't bother
						if (iAnarchyCost > iBenefit)
						{
							bDoRevolution = false;
						}
					}
					else if (gPlayerLogLevel > 0)
					{
						logBBAI("Waiting for near future civics before switching");
					}

					if (!bDoRevolution)
					{
						//	Check that a cheaper change is not worthwhile - remove the least-efficient civic option
						//	switch that is in the proposed set and check again
						int					iLowestEfficiency = MAX_INT;
						CivicOptionTypes	eWorstOptionSwitch = NO_CIVICOPTION;

						for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
						{
							if (paeBestCivic[iI] != getCivics((CivicOptionTypes)iI))
							{
								if (paeBestCivic[iI] != NO_CIVIC)
								{
									int anarchyLen = GC.getCivicInfo(paeBestCivic[iI]).getAnarchyLength();

									if (anarchyLen > 0)
									{
										int	efficiency = (paeBestCivicValue[iI] - paeCurCivicValue[iI]) / anarchyLen;

										if (efficiency < iLowestEfficiency)
										{
											iLowestEfficiency = efficiency;
											eWorstOptionSwitch = (CivicOptionTypes)iI;
										}
									}
									else if (iLowestEfficiency == MAX_INT)
									{
										eWorstOptionSwitch = (CivicOptionTypes)iI;
									}
								}
							}
						}

						FAssert(eWorstOptionSwitch != NO_CIVICOPTION);

						paeBestCivic[eWorstOptionSwitch] = getCivics(eWorstOptionSwitch);
						iCivicChanges--;
						iBestCivicsValue -= (paeBestCivicValue[eWorstOptionSwitch] - paeCurCivicValue[eWorstOptionSwitch]);
					}
				}
			}
		}
		else if (gPlayerLogLevel > 0)
		{
			logBBAI("No anarchy so switching");
		}
	}

	if (bDoRevolution && canRevolution(paeBestCivic))
	{
		m_iCivicSwitchMinDeltaThreshold = (iBestCivicsValue - iCurCivicsValue) * 2;
		revolution(paeBestCivic);
		AI_setCivicTimer((getMaxAnarchyTurns() == 0 || isGoldenAge()) ? GC.getDefineINT("MIN_REVOLUTION_TURNS") : CIVIC_CHANGE_DELAY);
	}
	else
	{
		// AI will re-evaluate whenever it gets a tech anyway, but if it's in a long stagant
		// period have it do so periodically anyway (but not so often as to create a peformance overhead)
		AI_setCivicTimer(CIVIC_CHANGE_DELAY);
	}

	m_iCityGrowthValueBase = -1;

	SAFE_DELETE_ARRAY(paeBestCivic);
	SAFE_DELETE_ARRAY(paeBestCivicValue);
	SAFE_DELETE_ARRAY(paeBestNearFutureCivicValue);
	SAFE_DELETE_ARRAY(paeCurCivicValue);
	SAFE_DELETE_ARRAY(paiAvailableChoices);
}


void CvPlayerAI::AI_doReligion()
{
	PROFILE_FUNC();

	ReligionTypes eBestReligion;

	FAssertMsg(!isHumanPlayer(), "isHuman did not return false as expected");

	if (isNPC())
	{
		return;
	}

	if (AI_getReligionTimer() > 0)
	{
		AI_changeReligionTimer(-1);
		return;
	}

	if (!canChangeReligion())
	{
		return;
	}

	FAssertMsg(AI_getReligionTimer() == 0, "AI Religion timer is expected to be 0");

	eBestReligion = AI_bestReligion();

	if (eBestReligion == NO_RELIGION)
	{
		eBestReligion = getStateReligion();
	}

	if (canConvert(eBestReligion))
	{
		convert(eBestReligion);
		AI_setReligionTimer((getMaxAnarchyTurns() == 0) ? (GC.getDefineINT("MIN_CONVERSION_TURNS") * 2) : RELIGION_CHANGE_DELAY);
	}
}


void CvPlayerAI::AI_beginDiplomacy(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	if (isDoNotBotherStatus(ePlayer))
	{
		// Divert AI diplomacy away from the diplomacy screen and induce the appropriate reaction
		// in the AI equivalent to a human rejecting the AI's requests in the interface. There are
		// a number of AI requests that do not need handling and that simply time out. There are
		// also AI requests that occur in CvTeam that induce the diplomacy screen in any case.
		// This diplomacy modification does not alter the AI's characteristics at all and is actually
		// just an interface modification for a player to shut down talks with an AI automatically.
		int ai_request;
		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_RELIGION_PRESSURE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_CONVERT, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_PRESSURE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_REVOLUTION, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_JOIN_WAR, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_STOP_TRADING");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_NO_STOP_TRADING, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_ASK_FOR_HELP");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_REFUSED_HELP, ePlayer, -1, -1);
		}

		ai_request = (DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE");
		if (ai_request == pDiploParams->getDiploComment())
		{
			this->handleDiploEvent(DIPLOEVENT_REJECTED_DEMAND, ePlayer, -1, -1);
			if (AI_demandRebukedWar(ePlayer))
			{
				this->handleDiploEvent(DIPLOEVENT_DEMAND_WAR, ePlayer, -1, -1);
			}
		}
	}
	else
	{
		gDLL->beginDiplomacy(pDiploParams, (PlayerTypes)ePlayer);
	}
}


void CvPlayerAI::AI_doDiplo()
{
	PROFILE_FUNC();

	FAssert(!isHumanPlayer());
	FAssert(!isMinorCiv());
	FAssert(!isNPC());

	CLLNode<TradeData>* pNode;
	CvDiploParameters* pDiplo;

	CLinkList<TradeData> ourList;
	CLinkList<TradeData> theirList;
	TradeData item;
	BonusTypes eBestReceiveBonus;
	BonusTypes eBestGiveBonus;
	TechTypes eBestReceiveTech;
	TechTypes eBestGiveTech;
	TeamTypes eBestTeam;

	int iBestValue;
	int iOurValue;
	int iLoop;

	bool abContacted[MAX_TEAMS];
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		abContacted[iI] = false;
	}
	const int iRandomTechChoiceSeed = GC.getGame().getSorenRandNum(GC.getNumTechInfos(), "AI trade random tech choice seed");
	stdext::hash_map<int, int> receivableTechs;

	{
		PROFILE("CvPlayerAI::AI_doDiplo.preCalcTechSources");

		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (GET_PLAYER((PlayerTypes)iI).isAlive() && iI != getID()
			&& canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI))
			{
				for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
				{
					setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

					if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
					{
						stdext::hash_map<int, int>::const_iterator itr = receivableTechs.find(iJ);

						receivableTechs[iJ] = itr != receivableTechs.end() ? itr->second + 1 : 1;
					}
				}
			}
		}

		for (stdext::hash_map<int, int>::const_iterator itr = receivableTechs.begin(); itr != receivableTechs.end(); ++itr)
		{
			logBBAI("Receivable tech %S has %d sources", GC.getTechInfo((TechTypes)itr->first).getDescription(), itr->second);
		}
	}

	for (int iPass = 0; iPass < 2; iPass++)
	{
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			if (iI == getID() || !GET_PLAYER((PlayerTypes)iI).isAlive())
			{
				continue;
			}
			if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() != (iPass == 1))
			{
				continue;
			}
			logBBAI("Player %d trade calc to/from player %d", getID(), iI);

			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				PROFILE("CvPlayerAI::AI_doDiplo.Existing");

				foreach_(CvDeal & kLoopDeal, GC.getGame().deals())
				{
					if (kLoopDeal.isCancelable(getID())
					&& GC.getGame().getGameTurn() - kLoopDeal.getInitialGameTurn() >= getTreatyLength() * 2)
					{
						bool bCancelDeal = false;

						if (kLoopDeal.getFirstPlayer() == getID() && kLoopDeal.getSecondPlayer() == (PlayerTypes)iI)
						{
							if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								for (pNode = kLoopDeal.getFirstTrades()->head(); pNode; pNode = kLoopDeal.getFirstTrades()->next(pNode))
								{
									if (getTradeDenial((PlayerTypes)iI, pNode->m_data) != NO_DENIAL)
									{
										bCancelDeal = true;
										break;
									}
								}
							}
							else if (!AI_considerOffer((PlayerTypes)iI, kLoopDeal.getSecondTrades(), kLoopDeal.getFirstTrades(), -1))
							{
								bCancelDeal = true;
							}
						}
						else if (kLoopDeal.getFirstPlayer() == (PlayerTypes)iI && kLoopDeal.getSecondPlayer() == getID())
						{
							if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								for (pNode = kLoopDeal.getSecondTrades()->head(); pNode; pNode = kLoopDeal.getSecondTrades()->next(pNode))
								{
									if (getTradeDenial(((PlayerTypes)iI), pNode->m_data) != NO_DENIAL)
									{
										bCancelDeal = true;
										break;
									}
								}
							}
							else if (!AI_considerOffer((PlayerTypes)iI, kLoopDeal.getFirstTrades(), kLoopDeal.getSecondTrades(), -1))
							{
								bCancelDeal = true;
							}
						}

						if (bCancelDeal)
						{
							if (canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI) && GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								ourList.clear();
								theirList.clear();

								for (pNode = kLoopDeal.headFirstTradesNode(); (pNode != NULL); pNode = kLoopDeal.nextFirstTradesNode(pNode))
								{
									if (kLoopDeal.getFirstPlayer() == getID())
									{
										ourList.insertAtEnd(pNode->m_data);
									}
									else theirList.insertAtEnd(pNode->m_data);
								}

								for (pNode = kLoopDeal.headSecondTradesNode(); (pNode != NULL); pNode = kLoopDeal.nextSecondTradesNode(pNode))
								{
									if (kLoopDeal.getSecondPlayer() == getID())
									{
										ourList.insertAtEnd(pNode->m_data);
									}
									else theirList.insertAtEnd(pNode->m_data);
								}

								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");

								if (kLoopDeal.isVassalDeal())
								{
									pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_NO_VASSAL"));
									pDiplo->setAIContact(true);

									AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								}
								else
								{
									pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CANCEL_DEAL"));
									pDiplo->setAIContact(true);
									pDiplo->setOurOfferList(theirList);
									pDiplo->setTheirOfferList(ourList);
									AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								}
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}

							if (kLoopDeal.isEmbassy())
							{
								for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
								{
									if (GET_PLAYER((PlayerTypes)iJ).isAlive() && getTeam() == GET_PLAYER((PlayerTypes)iJ).getTeam())
									{
										GET_PLAYER((PlayerTypes)iJ).AI_changeMemoryCount(((PlayerTypes)iI), MEMORY_RECALLED_AMBASSADOR, -AI_getMemoryCount(((PlayerTypes)iI), MEMORY_RECALLED_AMBASSADOR));
									}
								}
							}
							kLoopDeal.kill(); // XXX test this for AI...
						}
					}
				}
			}

			if (canContact((PlayerTypes)iI) && AI_isWillingToTalk((PlayerTypes)iI))
			{
				PROFILE("CvPlayerAI::AI_doDiplo.CanContact");

				if (GET_PLAYER((PlayerTypes)iI).getTeam() == getTeam() || GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
				{
					PROFILE("CvPlayerAI::AI_doDiplo.BonusTrade");

					// XXX will it cancel this deal if it loses it's first resource???

					iBestValue = 0;
					eBestGiveBonus = NO_BONUS;

					for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
					{
						if (getNumTradeableBonuses((BonusTypes)iJ) > 1
						&& GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal((BonusTypes)iJ, getID(), 1) > 0
						&& GET_PLAYER((PlayerTypes)iI).AI_bonusVal((BonusTypes)iJ, 1) > AI_bonusVal((BonusTypes)iJ, -1))
						{
							setTradeItem(&item, TRADE_RESOURCES, iJ);

							if (canTradeItem(((PlayerTypes)iI), item, true))
							{
								const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Bonus Trading #1");

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestGiveBonus = ((BonusTypes)iJ);
								}
							}
						}
					}

					if (eBestGiveBonus != NO_BONUS)
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
						ourList.insertAtEnd(item);

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
								pDiplo->setAIContact(true);
								pDiplo->setTheirOfferList(ourList);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
					}
				}

				if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
				{
					PROFILE("CvPlayerAI::AI_doDiplo.TechTrade");

					iBestValue = 0;
					eBestGiveTech = NO_TECH;

					// Don't give techs for free to advanced vassals ...
					if (GET_PLAYER((PlayerTypes)iI).getTechScore() * 10 < getTechScore() * 9)
					{
						for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
						{
							if (GET_TEAM(getTeam()).AI_techTrade((TechTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam()) == NO_DENIAL)
							{
								setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

								if (canTradeItem(((PlayerTypes)iI), item, true))
								{
									const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Vassal Tech gift");

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										eBestGiveTech = ((TechTypes)iJ);
									}
								}
							}
						}
					}

					if (eBestGiveTech != NO_TECH)
					{
						ourList.clear();
						theirList.clear();

						setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
						ourList.insertAtEnd(item);

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
						{
							if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
								pDiplo->setAIContact(true);
								pDiplo->setTheirOfferList(ourList);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}
						else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
					}
				}

				if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && !GET_TEAM(getTeam()).isHuman() && (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || !GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHuman()))
				{
					FAssertMsg(!(GET_PLAYER((PlayerTypes)iI).isNPC()), "(GET_PLAYER((PlayerTypes)iI).isNPC()) did not return false as expected");
					FAssertMsg(iI != getID(), "iI is not expected to be equal with getID()");

					if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isVassal(getTeam()))
					{
						PROFILE("CvPlayerAI::AI_doDiplo.Vasal.BonusTrade");

						iBestValue = 0;
						eBestGiveBonus = NO_BONUS;

						for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
						{
							if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 0 && getNumAvailableBonuses((BonusTypes)iJ) == 0)
							{
								const int iValue = AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									eBestGiveBonus = ((BonusTypes)iJ);
								}
							}
						}

						if (eBestGiveBonus != NO_BONUS)
						{
							theirList.clear();
							ourList.clear();

							setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
							theirList.insertAtEnd(item);

							if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
							{
								if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
								{
									CvPopupInfo* pInfo = new CvPopupInfo(BUTTONPOPUP_VASSAL_GRANT_TRIBUTE, getID(), eBestGiveBonus);
									if (pInfo)
									{
										gDLL->getInterfaceIFace()->addPopup(pInfo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}
							}
							else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
						}
					}

					if (!GET_TEAM(getTeam()).isAtWar(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						if (AI_getAttitude((PlayerTypes)iI) >= ATTITUDE_CAUTIOUS)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Cities");

							foreach_(const CvCity * pLoopCity, cities())
							{
								if (pLoopCity->getPreviousOwner() != (PlayerTypes)iI
								&& (pLoopCity->getGameTurnAcquired() + 4) % 20 == GC.getGame().getGameTurn() % 20)
								{
									int iCount = 0;
									int iPossibleCount = 0;

									for (int iJ = 0; iJ < NUM_CITY_PLOTS; iJ++)
									{
										CvPlot* pLoopPlot = plotCity(pLoopCity->getX(), pLoopCity->getY(), iJ);

										if (pLoopPlot != NULL)
										{
											if (pLoopPlot->getOwner() == iI)
											{
												iCount++;
											}
											iPossibleCount++;
										}
									}

									if (iCount >= iPossibleCount / 2)
									{
										setTradeItem(&item, TRADE_CITIES, pLoopCity->getID());

										if (canTradeItem(((PlayerTypes)iI), item, true))
										{
											ourList.clear();
											ourList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_CITY"));
												pDiplo->setAIContact(true);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
											else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, NULL);
										}
									}
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.PermAlliance");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE) == 0)
							{
								bool bOffered = false;
								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PERMANENT_ALLIANCE), "AI Diplo Alliance") == 0)
								{
									setTradeItem(&item, TRADE_PERMANENT_ALLIANCE);

									if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
									{
										ourList.clear();
										theirList.clear();

										ourList.insertAtEnd(item);
										theirList.insertAtEnd(item);

										bOffered = true;

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PERMANENT_ALLIANCE));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											break; // move on to next player since we are on the same team now
										}
									}
								}

								if (!bOffered)
								{
									setTradeItem(&item, TRADE_VASSAL);

									if (canTradeItem((PlayerTypes)iI, item, true))
									{
										ourList.clear();
										theirList.clear();

										ourList.insertAtEnd(item);

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PERMANENT_ALLIANCE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PERMANENT_ALLIANCE));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_VASSAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);

												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											const TeamTypes eMasterTeam = GET_PLAYER((PlayerTypes)iI).getTeam();
											bool bAccepted = true;

											for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
											{
												if (GET_TEAM((TeamTypes)iJ).isAlive()
												&& iJ != getTeam() && iJ != eMasterTeam
												&& atWar(getTeam(), (TeamTypes)iJ)
												&& !atWar(eMasterTeam, (TeamTypes)iJ)
												&& GET_TEAM(eMasterTeam).AI_declareWarTrade((TeamTypes)iJ, getTeam(), false) != NO_DENIAL)
												{
													bAccepted = false;
													break;
												}
											}
											if (bAccepted)
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Religion");

							if (getStateReligion() != NO_RELIGION
							&& GET_PLAYER((PlayerTypes)iI).canConvert(getStateReligion())
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_RELIGION_PRESSURE) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_RELIGION_PRESSURE), "AI Diplo Religion Pressure") == 0
							&& !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_RELIGION_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_RELIGION_PRESSURE));
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_RELIGION_PRESSURE"));
								pDiplo->setAIContact(true);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Civic");

							const CivicTypes eFavoriteCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();

							if (eFavoriteCivic != NO_CIVIC && isCivic(eFavoriteCivic)
							&& GET_PLAYER((PlayerTypes)iI).canDoCivics(eFavoriteCivic)
							&& !GET_PLAYER((PlayerTypes)iI).isCivic(eFavoriteCivic)
							&& GET_PLAYER((PlayerTypes)iI).canRevolution(NULL)
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_CIVIC_PRESSURE) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_CIVIC_PRESSURE), "AI Diplo Civic Pressure") == 0
							&& !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
							{
								AI_changeContactTimer(((PlayerTypes)iI), CONTACT_CIVIC_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_CIVIC_PRESSURE));
								pDiplo = new CvDiploParameters(getID());
								FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
								pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_CIVIC_PRESSURE"), GC.getCivicInfo(eFavoriteCivic).getTextKeyWide());
								pDiplo->setAIContact(true);

								AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
								abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && GC.getDefineINT("CAN_TRADE_WAR") > 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.WarWith");

							if (AI_getMemoryCount(((PlayerTypes)iI), MEMORY_DECLARED_WAR) == 0 && AI_getMemoryCount(((PlayerTypes)iI), MEMORY_HIRED_WAR_ALLY) == 0)
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_JOIN_WAR) == 0)
								{
									int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_JOIN_WAR);
									AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
									if (eAttitude != ATTITUDE_FRIENDLY)
									{
										iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
									}
									if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Join War") == 0)
									{
										iBestValue = 0;
										eBestTeam = NO_TEAM;

										for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
										{
											if (GET_TEAM((TeamTypes)iJ).isAlive()
											&& atWar((TeamTypes)iJ, getTeam())
											&& !atWar((TeamTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam())
											&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHasMet((TeamTypes)iJ)
											&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).canDeclareWar((TeamTypes)iJ))
											{
												const int iValue = (1 + GC.getGame().getSorenRandNum(10000, "AI Joining War"));

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestTeam = ((TeamTypes)iJ);
												}
											}
										}

										if (eBestTeam != NO_TEAM && !abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											m_eDemandWarAgainstTeam = eBestTeam;

											AI_changeContactTimer((PlayerTypes)iI, CONTACT_JOIN_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_JOIN_WAR));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
											pDiplo->setAIContact(true);
											pDiplo->setData(eBestTeam);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.StopTradingWith");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_STOP_TRADING) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_STOP_TRADING);
								const AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
								if (eAttitude != ATTITUDE_FRIENDLY)
								{
									iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
								}
								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Stop Trading") == 0)
								{
									if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
									{
										eBestTeam = AI_bestStopTradeTeam((PlayerTypes)iI);
									}
									else
									{
										eBestTeam = GET_TEAM(getTeam()).AI_getWorstEnemy();
									}

									if (eBestTeam != NO_TEAM && GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isHasMet(eBestTeam)
									&& !GET_TEAM(eBestTeam).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())
									&& GET_PLAYER((PlayerTypes)iI).canStopTradingWithTeam(eBestTeam))
									{
										FAssert(!atWar(GET_PLAYER((PlayerTypes)iI).getTeam(), eBestTeam));
										FAssert(GET_PLAYER((PlayerTypes)iI).getTeam() != eBestTeam);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_STOP_TRADING, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_STOP_TRADING));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_STOP_TRADING"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
											pDiplo->setAIContact(true);
											pDiplo->setData(eBestTeam);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Help");

							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getAssets() < GET_TEAM(getTeam()).getAssets() / 2
							&& AI_getAttitude((PlayerTypes)iI) > GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getNoGiveHelpAttitudeThreshold()
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_GIVE_HELP) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_GIVE_HELP), "AI Diplo Give Help") == 0)
							{
								// XXX maybe do gold instead???

								iBestValue = 0;
								eBestGiveTech = NO_TECH;

								for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
								{
									setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

									if (canTradeItem((PlayerTypes)iI, item, true))
									{
										const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Giving Help");

										if (iValue > iBestValue)
										{
											iBestValue = iValue;
											eBestGiveTech = ((TechTypes)iJ);
										}
									}
								}

								if (eBestGiveTech != NO_TECH)
								{
									ourList.clear();

									setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
									ourList.insertAtEnd(item);

									if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
									{
										AI_changeContactTimer(((PlayerTypes)iI), CONTACT_GIVE_HELP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_GIVE_HELP));
										pDiplo = new CvDiploParameters(getID());
										FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
										pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_GIVE_HELP"));
										pDiplo->setAIContact(true);
										pDiplo->setTheirOfferList(ourList);
										AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && (GET_TEAM(getTeam()).getLeaderID() == getID()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AskHelp");

							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getAssets() > GET_TEAM(getTeam()).getAssets() / 2
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_ASK_FOR_HELP) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_ASK_FOR_HELP);
								const int iTechPerc = GET_TEAM(getTeam()).getBestKnownTechScorePercent();
								if (iTechPerc < 90)
								{
									iRand *= std::max(1, iTechPerc - 60);
									iRand /= 30;
								}

								//Afforess make unfriendly AI's less likely to ask for help
								AttitudeTypes eAttitude = AI_getAttitude((PlayerTypes)iI);
								if (eAttitude != ATTITUDE_FRIENDLY)
								{
									iRand *= (eAttitude == ATTITUDE_PLEASED ? 10 : 100);
								}

								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Ask For Help") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
									{
										TechTypes eCandidateTech = (TechTypes)((iRandomTechChoiceSeed + iJ) % GC.getNumTechInfos());
										setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											eBestReceiveTech = eCandidateTech;
											break;
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_ASK_FOR_HELP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_ASK_FOR_HELP));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_ASK_FOR_HELP"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() && GET_TEAM(getTeam()).getLeaderID() == getID()
						&& GET_TEAM(getTeam()).canDeclareWar(GET_PLAYER((PlayerTypes)iI).getTeam())
						&& !GET_TEAM(getTeam()).AI_isChosenWar(GET_PLAYER((PlayerTypes)iI).getTeam()))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.Tribute");

							//Afforess changed to check if we are at least 1.5x as powerful
							if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true) * 3 < GET_TEAM(getTeam()).getPower(true) * 2
							&& AI_getAttitude((PlayerTypes)iI) <= GC.getLeaderHeadInfo(GET_PLAYER((PlayerTypes)iI).getPersonalityType()).getDemandTributeAttitudeThreshold()
							&& AI_getContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE) == 0)
							{
								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									int64_t iReceiveGold = std::min<int64_t>(std::max<int64_t>(0, GET_PLAYER((PlayerTypes)iI).getGold() - 50), GET_PLAYER((PlayerTypes)iI).AI_goldTarget());

									iReceiveGold -= (iReceiveGold % GC.getDefineINT("DIPLOMACY_VALUE_REMAINDER"));

									if (iReceiveGold > 50)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_GOLD, iReceiveGold > MAX_INT ? MAX_INT : (int)iReceiveGold);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0
								&& GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER((PlayerTypes)iI).getTeam()) > 100)
								{
									theirList.clear();

									setTradeItem(&item, TRADE_MAPS);
									theirList.insertAtEnd(item);

									if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
									{
										AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
										pDiplo = new CvDiploParameters(getID());
										FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
										pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
										pDiplo->setAIContact(true);
										pDiplo->setOurOfferList(theirList);

										AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
										abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
									{
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true)
										&& GC.getGame().countKnownTechNumTeams((TechTypes)iJ) > 1)
										{
											const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Demanding Tribute (Tech)");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestReceiveTech = ((TechTypes)iJ);
											}
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEMAND_TRIBUTE), "AI Diplo Demand Tribute") == 0)
								{
									iBestValue = 0;
									eBestReceiveBonus = NO_BONUS;

									for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
									{
										if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 1
										&& AI_bonusTradeVal(((BonusTypes)iJ), ((PlayerTypes)iI), 1) > 0)
										{
											setTradeItem(&item, TRADE_RESOURCES, iJ);

											if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Demanding Tribute (Bonus)");

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestReceiveBonus = ((BonusTypes)iJ);
												}
											}
										}
									}

									if (eBestReceiveBonus != NO_BONUS)
									{
										theirList.clear();

										setTradeItem(&item, TRADE_RESOURCES, eBestReceiveBonus);
										theirList.insertAtEnd(item);

										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEMAND_TRIBUTE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEMAND_TRIBUTE));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_DEMAND_TRIBUTE"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.OpenBorders");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Open Borders") == 0)
							{
								setTradeItem(&item, TRADE_OPEN_BORDERS);

								if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									ourList.clear();
									theirList.clear();

									ourList.insertAtEnd(item);
									theirList.insertAtEnd(item);

									if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
									{
										if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											pDiplo->setTheirOfferList(ourList);

											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
									else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
								}
							}
						}

						if (GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.DefensivePact");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_DEFENSIVE_PACT) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_DEFENSIVE_PACT), "AI Diplo Defensive Pact") == 0)
							{
								setTradeItem(&item, TRADE_DEFENSIVE_PACT);

								if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
								{
									ourList.clear();
									theirList.clear();

									ourList.insertAtEnd(item);
									theirList.insertAtEnd(item);

									if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
									{
										if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
										{
											AI_changeContactTimer(((PlayerTypes)iI), CONTACT_DEFENSIVE_PACT, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_DEFENSIVE_PACT));
											pDiplo = new CvDiploParameters(getID());
											FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
											pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
											pDiplo->setAIContact(true);
											pDiplo->setOurOfferList(theirList);
											pDiplo->setTheirOfferList(ourList);
											AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
											abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
										}
									}
									else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
								}
							}
						}

						if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || GET_TEAM(getTeam()).getLeaderID() == getID())
						{
							PROFILE("CvPlayerAI::AI_doDiplo.TradeTechNonHuman");

							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_TECH) == 0)
							{
								int iRand = GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_TECH);
								const int iTechPerc = GET_TEAM(getTeam()).getBestKnownTechScorePercent();
								if (iTechPerc < 90)
								{
									iRand *= std::max(1, iTechPerc - 60);
									iRand /= 30;
								}
								if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE1))
								{
									iRand /= 2;
								}

								iRand = std::max(1, iRand);
								if (GC.getGame().getSorenRandNum(iRand, "AI Diplo Trade Tech") == 0)
								{
									iBestValue = 0;
									eBestReceiveTech = NO_TECH;

									for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
									{
										TechTypes eCandidateTech = (TechTypes)((iRandomTechChoiceSeed + iJ) % GC.getNumTechInfos());
										setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											stdext::hash_map<int, int>::const_iterator itr = receivableTechs.find(eCandidateTech);

											const int iValue = itr != receivableTechs.end() ? itr->second : 0;

											if (eBestReceiveTech == NO_TECH || iValue > iBestValue)
											{
												eBestReceiveTech = eCandidateTech;
												iBestValue = iValue;
											}
										}
									}

									if (eBestReceiveTech != NO_TECH)
									{
										iBestValue = 0;
										eBestGiveTech = NO_TECH;

										for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
										{
											TechTypes eCandidateTech = (TechTypes)((iRandomTechChoiceSeed + iJ) % GC.getNumTechInfos());
											setTradeItem(&item, TRADE_TECHNOLOGIES, eCandidateTech);

											if (canTradeItem(((PlayerTypes)iI), item, true))
											{
												eBestGiveTech = eCandidateTech;
												break;
											}
										}

										iOurValue = GET_TEAM(getTeam()).AI_techTradeVal(eBestReceiveTech, GET_PLAYER((PlayerTypes)iI).getTeam());
										int iTheirValue =
											(
												eBestGiveTech != NO_TECH
												?
												GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
												:
												0
											);

										int iReceiveGold = 0;
										int iGiveGold = 0;

										if (iTheirValue > iOurValue)
										{
											const int iValueDiff = iTheirValue - iOurValue;
											const int iGoldValuePercent = AI_goldTradeValuePercent();
											int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
											if (iGold > 0)
											{
												const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

												if (iGold > iMaxTrade)
												{
													iGold = iMaxTrade;
												}
												else
												{
													// Account for rounding errors
													while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
													{
														iGold++;
													}
												}

												if (iGold > 0)
												{
													const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
													if (iValue > 0)
													{
														setTradeItem(&item, TRADE_GOLD, iGold);

														if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
														{
															iReceiveGold = iGold;
															iOurValue += iValue;
														}
													}
												}
											}
										}
										else if (iOurValue > iTheirValue)
										{
											const int iValueDiff = iOurValue - iTheirValue;
											const int iGoldValuePercent = AI_goldTradeValuePercent();
											int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
											if (iGold > 0)
											{
												const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

												if (iGold > iMaxTrade)
												{
													iGold = iMaxTrade;
												}
												else
												{
													// Account for rounding errors
													while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
													{
														iGold++;
													}
												}

												if (iGold > 0)
												{
													const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
													if (iValue > 0)
													{
														setTradeItem(&item, TRADE_GOLD, iGold);

														if (canTradeItem((PlayerTypes)iI, item, true))
														{
															iGiveGold = iGold;
															iTheirValue += iValue;
														}
													}
												}
											}
										}

										if ((!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
										&& iOurValue > iTheirValue * 2 / 3 && iTheirValue > iOurValue * 2 / 3)
										{
											ourList.clear();
											theirList.clear();

											if (eBestGiveTech != NO_TECH)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
												ourList.insertAtEnd(item);
											}

											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestReceiveTech);
											theirList.insertAtEnd(item);

											if (iGiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iGiveGold);
												ourList.insertAtEnd(item);
											}

											if (iReceiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iReceiveGold);
												theirList.insertAtEnd(item);
											}

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_TECH, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_TECH));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);

													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}

						if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY) || GC.getGame().isOption(GAMEOPTION_AI_RUTHLESS))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AdvancedDiplomacyOrRuthless");

							//Purchase War Ally
							if (
								(
									GET_PLAYER((PlayerTypes)iI).isHumanPlayer()
									||
									GET_TEAM(getTeam()).getLeaderID() == getID()
									&&
									!GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())
									)
							&& AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR) == 0
							&& AI_getMemoryCount((PlayerTypes)iI, MEMORY_HIRED_WAR_ALLY) == 0
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_TRADE_JOIN_WAR) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_JOIN_WAR), "AI Diplo Trade War") == 0)
							{
								const TeamTypes eBestWarTeam = AI_bestJoinWarTeam((PlayerTypes)iI);

								if (eBestWarTeam != NO_TEAM
								&& GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_declareWarTrade(eBestWarTeam, getTeam(), true) == NO_DENIAL
								&& (
									GET_TEAM(getTeam()).isGoldTrading()
									||
									GET_TEAM(getTeam()).isTechTrading()
									||
									GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isGoldTrading()
									||
									GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isTechTrading()
									)
								)
								{
									iBestValue = 0;
									eBestGiveTech = NO_TECH;

									for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
									{
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Tech Trading #2");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestGiveTech = (TechTypes)iJ;
											}
										}
									}

									iOurValue = GET_TEAM(getTeam()).AI_declareWarTradeVal(eBestWarTeam, GET_PLAYER((PlayerTypes)iI).getTeam());
									int iTheirValue =
										(
											eBestGiveTech != NO_TECH
											?
											GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
											:
											0
										);

									int iReceiveGold = 0;
									int iGiveGold = 0;

									if (iTheirValue > iOurValue)
									{
										const int iValueDiff = iTheirValue - iOurValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
													{
														iReceiveGold = iGold;
														iOurValue += iValue;
													}
												}
											}
										}
									}
									else if (iOurValue > iTheirValue)
									{
										const int iValueDiff = iOurValue - iTheirValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (canTradeItem((PlayerTypes)iI, item, true))
													{
														iGiveGold = iGold;
														iTheirValue += iValue;
													}
												}
											}
										}
									}

									if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
									{
										if ((iOurValue > ((iTheirValue * 2) / 3)) && (iTheirValue > ((iOurValue * 2) / 3)))
										{
											ourList.clear();
											theirList.clear();

											if (eBestGiveTech != NO_TECH)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
												ourList.insertAtEnd(item);
											}

											setTradeItem(&item, TRADE_WAR, eBestWarTeam);
											theirList.insertAtEnd(item);

											if (iGiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iGiveGold);
												ourList.insertAtEnd(item);
											}

											if (iReceiveGold != 0)
											{
												setTradeItem(&item, TRADE_GOLD, iReceiveGold);
												theirList.insertAtEnd(item);
											}

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													m_eDemandWarAgainstTeam = eBestWarTeam;
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_JOIN_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_JOIN_WAR));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestWarTeam).getLeaderID()).getCivilizationAdjectiveKey());
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else if (GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_declareWarTrade(eBestWarTeam, getTeam(), true) == NO_DENIAL)
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
											else m_eDemandWarAgainstTeam = eBestWarTeam;
										}
									}
								}
							}
							//Broker Peace
							if (GET_TEAM(getTeam()).getLeaderID() == getID() && !GC.getGame().isPreviousRequest((PlayerTypes)iI))
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_PEACE_PRESSURE) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_PEACE_PRESSURE), "AI Diplo End War") == 0)
									{
										eBestTeam = AI_bestMakePeaceTeam((PlayerTypes)iI);

										if (eBestTeam != NO_TEAM)
										{
											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													GC.getGame().setPreviousRequest((PlayerTypes)iI, true);
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_PEACE_PRESSURE, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_PEACE_PRESSURE));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_MAKE_PEACE_WITH"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
													pDiplo->setAIContact(true);
													pDiplo->setData(eBestTeam);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Purchase Trade Embargo
							if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || ((GET_TEAM(getTeam()).getLeaderID() == getID()) && !GET_TEAM(getTeam()).isVassal(GET_PLAYER((PlayerTypes)iI).getTeam())))
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_STOP_TRADING) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_JOIN_WAR), "AI Diplo Trade War") == 0)
									{
										const TeamTypes eBestStopTradeTeam = AI_bestStopTradeTeam((PlayerTypes)iI);

										if (eBestStopTradeTeam != NO_TEAM)
										{

											iBestValue = 0;
											eBestGiveTech = NO_TECH;

											for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

												if (canTradeItem((PlayerTypes)iI, item, true))
												{
													const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Tech Trading #2");

													if (iValue > iBestValue)
													{
														iBestValue = iValue;
														eBestGiveTech = ((TechTypes)iJ);
													}
												}
											}

											iOurValue = AI_stopTradingTradeVal(eBestStopTradeTeam, ((PlayerTypes)iI));
											int iTheirValue =
												(
													eBestGiveTech != NO_TECH
													?
													GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam())
													:
													0
												);
											int iReceiveGold = 0;
											int iGiveGold = 0;

											if (iTheirValue > iOurValue)
											{
												const int iValueDiff = iTheirValue - iOurValue;
												const int iGoldValuePercent = AI_goldTradeValuePercent();
												int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
												if (iGold > 0)
												{
													const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

													if (iGold > iMaxTrade)
													{
														iGold = iMaxTrade;
													}
													else
													{
														// Account for rounding errors
														while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
														{
															iGold++;
														}
													}

													if (iGold > 0)
													{
														const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
														if (iValue > 0)
														{
															setTradeItem(&item, TRADE_GOLD, iGold);

															if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
															{
																iReceiveGold = iGold;
																iOurValue += iValue;
															}
														}
													}
												}
											}
											else if (iOurValue > iTheirValue)
											{
												const int iValueDiff = iOurValue - iTheirValue;
												const int iGoldValuePercent = AI_goldTradeValuePercent();
												int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
												if (iGold > 0)
												{
													const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

													if (iGold > iMaxTrade)
													{
														iGold = iMaxTrade;
													}
													else
													{
														// Account for rounding errors
														while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
														{
															iGold++;
														}
													}

													if (iGold > 0)
													{
														const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
														if (iValue > 0)
														{
															setTradeItem(&item, TRADE_GOLD, iGold);

															if (canTradeItem((PlayerTypes)iI, item, true))
															{
																iGiveGold = iGold;
																iTheirValue += iValue;
															}
														}
													}
												}
											}

											if ((!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || iOurValue >= iTheirValue)
											&& iOurValue > iTheirValue * 2 / 3 && iTheirValue > iOurValue * 2 / 3)
											{
												ourList.clear();
												theirList.clear();

												if (eBestGiveTech != NO_TECH)
												{
													setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
													ourList.insertAtEnd(item);
												}

												setTradeItem(&item, TRADE_EMBARGO, eBestStopTradeTeam);
												theirList.insertAtEnd(item);

												if (iGiveGold != 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGiveGold);
													ourList.insertAtEnd(item);
												}

												if (iReceiveGold != 0)
												{
													setTradeItem(&item, TRADE_GOLD, iReceiveGold);
													theirList.insertAtEnd(item);
												}

												if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
												{
													GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
												}
												else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_STOP_TRADING, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_STOP_TRADING));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
										}
									}
								}
							}
						}
						if (GC.getGame().isOption(GAMEOPTION_ADVANCED_DIPLOMACY))
						{
							PROFILE("CvPlayerAI::AI_doDiplo.AdvancedDiplomacy");

							//Establish Embassy
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (!GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam()))
								{
									if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_EMBASSY) == 0)
									{
										if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_EMBASSY), "AI Diplo Open Borders") == 0)
										{
											setTradeItem(&item, TRADE_EMBASSY);

											if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												ourList.clear();
												theirList.clear();

												ourList.insertAtEnd(item);
												theirList.insertAtEnd(item);

												if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
												{
													if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
													{
														AI_changeContactTimer(((PlayerTypes)iI), CONTACT_EMBASSY, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_EMBASSY));
														pDiplo = new CvDiploParameters(getID());
														FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
														pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
														pDiplo->setAIContact(true);
														pDiplo->setOurOfferList(theirList);
														pDiplo->setTheirOfferList(ourList);
														AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
														abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
													}
												}
												else
												{
													GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
												}
											}
										}
									}
								}
							}
							//Open Free Trade
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Limited Borders") == 0)
									{
										setTradeItem(&item, TRADE_FREE_TRADE_ZONE);

										if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											ourList.clear();
											theirList.clear();

											ourList.insertAtEnd(item);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													// RevolutionDCM start - new diplomacy option
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// gDLL->beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// RevolutionDCM end
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Open Limited Borders
							if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_OPEN_BORDERS), "AI Diplo Limited Borders") == 0)
									{
										setTradeItem(&item, TRADE_RITE_OF_PASSAGE);

										if (canTradeItem(((PlayerTypes)iI), item, true) && GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											ourList.clear();
											theirList.clear();

											ourList.insertAtEnd(item);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_OPEN_BORDERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_OPEN_BORDERS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													// RevolutionDCM start - new diplomacy option
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// gDLL->beginDiplomacy(pDiplo, (PlayerTypes)iI);
													// RevolutionDCM end
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
							//Sell contacts to other teams
							if (GET_TEAM(getTeam()).getLeaderID() == getID()
							&& AI_getContactTimer((PlayerTypes)iI, CONTACT_TRADE_CONTACTS) == 0
							&& GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_CONTACTS) / 2, "AI Diplo Trade Contacts") == 0)
							{
								for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
								{
									const TeamTypes eTeamX = static_cast<TeamTypes>(iJ);
									CvTeam& kTeam = GET_TEAM(eTeamX);
									if (kTeam.isAlive() && !kTeam.isMinorCiv())
									{
										setTradeItem(&item, TRADE_CONTACT, eTeamX);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iGold = AI_getGoldFromValue(GET_TEAM(getTeam()).AI_contactTradeVal(eTeamX, GET_PLAYER((PlayerTypes)iI).getTeam()), AI_goldTradeValuePercent());

											if (iGold > 0 && iGold <= GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()))
											{
												setTradeItem(&item, TRADE_GOLD, iGold);

												if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
												{
													ourList.clear();
													theirList.clear();

													setTradeItem(&item, TRADE_CONTACT, eTeamX);
													ourList.insertAtEnd(item);

													setTradeItem(&item, TRADE_GOLD, iGold);
													theirList.insertAtEnd(item);

													if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
													{
														GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
														logging::logMsg("C2C.log", "Player %d has traded contact of %d to %d for %d gold\n", getID(), GET_TEAM(eTeamX).getLeaderID(), iI, iGold);
														CvWString szBuffer;
														switch (AI_getAttitude(GET_TEAM(eTeamX).getLeaderID()))
														{
														case ATTITUDE_FURIOUS:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_FURIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_ANNOYED:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_ANNOYED", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_CAUTIOUS:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_CAUTIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_PLEASED:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_PLEASED", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														case ATTITUDE_FRIENDLY:
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_FRIENDLY", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														default:
															FErrorMsg("No Valid Attitude");
															szBuffer = gDLL->getText("TXT_KEY_MISC_TRADED_CONTACT_CAUTIOUS", getCivilizationDescription(), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription());
															break;
														}
														for (int iJ = 0; iJ < MAX_PC_PLAYERS; iJ++)
														{
															if (GET_PLAYER((PlayerTypes)iJ).getTeam() == eTeamX)
															{
																AddDLLMessage((PlayerTypes)iJ, true, GC.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_FEAT_ACCOMPLISHED", MESSAGE_TYPE_MAJOR_EVENT, NULL, GC.getCOLOR_WHITE());
															}
														}
													}
													else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
													{
														AI_changeContactTimer((PlayerTypes)iI, CONTACT_TRADE_CONTACTS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_CONTACTS));
														pDiplo = new CvDiploParameters(getID());
														FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
														pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
														pDiplo->setAIContact(true);
														pDiplo->setOurOfferList(theirList);
														pDiplo->setTheirOfferList(ourList);
														AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
														abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
													}
												}
											}
											// Yes, this is the first team we can sell contact with...
											break; // Not like it really matters
										}
									}
								}
							}
							//Purchase Workers
							//Why Does this matter?
							//if (GET_TEAM(getTeam()).getLeaderID() == getID())
							{
								PROFILE("CvPlayerAI::AI_doDiplo.PurchaseWorker");

								if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_WORKERS) == 0)
								{
									if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_WORKERS), "AI Diplo Trade Workers") == 0)
									{
										if (GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam()))
										{
											CvUnit* pWorker = NULL;
											int iNeededWorkers = 0;

											//figure out if we need workers or not
											foreach_(CvArea * pLoopArea, GC.getMap().areas())
											{
												if (pLoopArea->getCitiesPerPlayer(getID()) > 0)
												{
													iNeededWorkers += AI_neededWorkers(pLoopArea);
												}
											}
											//if we need workers
											if (iNeededWorkers > 0)
											{
												foreach_(CvUnit * pLoopUnit, GET_PLAYER((PlayerTypes)iI).units())
												{
													if (pLoopUnit->canTradeUnit(getID()))
													{
														setTradeItem(&item, TRADE_WORKER, pLoopUnit->getID());
														//if they can trade the worker to us
														if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
														{
															pWorker = pLoopUnit;
															break;
														}
													}
												}
											}
											if (pWorker != NULL)
											{
												const int iGold = AI_getGoldFromValue(GET_PLAYER((PlayerTypes)iI).AI_workerTradeVal(pWorker), AI_goldTradeValuePercent());

												if (iGold > 0 && AI_maxGoldTrade((PlayerTypes)iI) >= iGold)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);
													//if we can trade the gold to them
													if (canTradeItem((PlayerTypes)iI, item, true))
													{
														ourList.clear();
														theirList.clear();

														ourList.insertAtEnd(item);

														setTradeItem(&item, TRADE_WORKER, pWorker->getID());
														theirList.insertAtEnd(item);

														if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
														{
															if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
															{
																AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_WORKERS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_WORKERS));
																pDiplo = new CvDiploParameters(getID());
																FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
																pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
																pDiplo->setAIContact(true);
																pDiplo->setOurOfferList(theirList);
																pDiplo->setTheirOfferList(ourList);
																AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
																abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
															}
														}
														else
														{
															GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
															//logging::logMsg("C2C.log", "Player %d has traded a worker to %d for %d gold\n", getID(), iI, iGold);
														}
													}
												}
											}
										}
									}
								}
							}

							// Trade military units
							if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MILITARY_UNITS) == 0)
							{
								PROFILE("CvPlayerAI::AI_doDiplo.TardeUnits");

								if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_MILITARY_UNITS)
									/ std::max(1, GET_TEAM(getTeam()).getAnyWarPlanCount(true)), "AI Diplo Trade Military Units") == 0

								&& GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER((PlayerTypes)iI).getTeam())
								&& !AI_isFinancialTrouble())
								{
									int* paiMilitaryUnits;
									paiMilitaryUnits = new int[GET_PLAYER((PlayerTypes)iI).getNumUnits()];
									for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
									{
										paiMilitaryUnits[iJ] = -1;
									}
									int iNumTradableUnits = 0;
									CvUnit* pLoopUnit;
									for (iJ = 0, pLoopUnit = GET_PLAYER((PlayerTypes)iI).firstUnit(&iLoop); pLoopUnit != NULL; iJ++, pLoopUnit = GET_PLAYER((PlayerTypes)iI).nextUnit(&iLoop))
									{
										if (pLoopUnit->canTradeUnit(getID()))
										{
											setTradeItem(&item, TRADE_MILITARY_UNIT, pLoopUnit->getID());
											if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
											{
												paiMilitaryUnits[iJ] = pLoopUnit->getID();
												iNumTradableUnits++;
											}
										}
									}
									TechTypes eBestTech = NO_TECH;
									int iBestValue = 0;
									if (iNumTradableUnits > 0)
									{
										for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
										{
											setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

											if (canTradeItem((PlayerTypes)iI, item, true))
											{
												const int iValue =
													(
														1 + GC.getGame().getSorenRandNum(10000, "AI Tech For Military")
														/
														std::max(1, GC.getTechInfo((TechTypes)iJ).getFlavorValue(GC.getInfoTypeForString("FLAVOR_MILITARY")))
													);
												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestTech = (TechTypes)iJ;
												}
											}
										}
									}
									if (eBestTech != NO_TECH)
									{
										int iUnitValue = 0;
										int iTechValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestTech, getTeam());
										for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
										{
											if (paiMilitaryUnits[iJ] > 0)
											{
												if (iUnitValue > iTechValue)
												{
													paiMilitaryUnits[iJ] = -1;
												}
												else
												{
													iUnitValue += AI_militaryUnitTradeVal(GET_PLAYER((PlayerTypes)iI).getUnit(paiMilitaryUnits[iJ]));
												}
											}
										}

										ourList.clear();
										theirList.clear();

										//Units are worth more than the tech
										if (iUnitValue > iTechValue)
										{
											const int iValueDiff = iUnitValue - iTechValue;
											const int iGold = AI_getGoldFromValue(iValueDiff, AI_goldTradeValuePercent());

											if (iGold > 0 && AI_maxGoldTrade((PlayerTypes)iI) >= iGold)
											{
												setTradeItem(&item, TRADE_GOLD, iGold);
												if (canTradeItem((PlayerTypes)iI, item, true))
												{
													ourList.insertAtEnd(item);
													iTechValue += iValueDiff;
												}
											}
										}
										//The tech is worth more than the units
										else if (iUnitValue < iTechValue)
										{
											const int iValueDiff = iTechValue - iUnitValue;
											const int iGold = AI_getGoldFromValue(iValueDiff, AI_goldTradeValuePercent());

											if (iGold > 0 && GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID()) >= iGold)
											{
												setTradeItem(&item, TRADE_GOLD, iGold);
												if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
												{
													theirList.insertAtEnd(item);
													iUnitValue += iValueDiff;
												}
											}
										}
										if (iUnitValue == iTechValue)
										{
											for (int iJ = 0; iJ < GET_PLAYER((PlayerTypes)iI).getNumUnits(); iJ++)
											{
												if (paiMilitaryUnits[iJ] > 0)
												{
													setTradeItem(&item, TRADE_MILITARY_UNIT, paiMilitaryUnits[iJ]);
													theirList.insertAtEnd(item);
												}
											}
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestTech);
											ourList.insertAtEnd(item);

											if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
												//logging::logMsg("C2C.log", "Player %d has traded military units to %d.\n", getID(), iI);
											}
											else if (!abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()])
											{
												AI_changeContactTimer((PlayerTypes)iI, CONTACT_TRADE_MILITARY_UNITS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_MILITARY_UNITS));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
									}
									SAFE_DELETE_ARRAY(paiMilitaryUnits);
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BONUS) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.ContactTradeBonus");

							if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_BONUS), "AI Diplo Trade Bonus") == 0)
							{
								iBestValue = 0;
								eBestReceiveBonus = NO_BONUS;

								for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
								{
									if (GET_PLAYER((PlayerTypes)iI).getNumTradeableBonuses((BonusTypes)iJ) > 1
									&& GET_PLAYER((PlayerTypes)iI).AI_corporationBonusVal((BonusTypes)iJ) == 0
									&& AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1) > 0)
									{
										setTradeItem(&item, TRADE_RESOURCES, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											const int iValue = AI_bonusTradeVal((BonusTypes)iJ, (PlayerTypes)iI, 1) + GC.getGame().getSorenRandNum(200, "AI Bonus Trading #1");

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestReceiveBonus = ((BonusTypes)iJ);
											}
										}
									}
								}

								if (eBestReceiveBonus != NO_BONUS)
								{
									iBestValue = 0;
									eBestGiveBonus = NO_BONUS;

									for (int iJ = 0; iJ < GC.getNumBonusInfos(); iJ++)
									{
										if (iJ != eBestReceiveBonus && getNumTradeableBonuses((BonusTypes)iJ) > 1
										&& GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal((BonusTypes)iJ, getID(), 1) > 0)
										{
											setTradeItem(&item, TRADE_RESOURCES, iJ);

											if (canTradeItem(((PlayerTypes)iI), item, true))
											{
												const int iValue = 1 + GC.getGame().getSorenRandNum(10000, "AI Bonus Trading #2");

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													eBestGiveBonus = (BonusTypes)iJ;
												}
											}
										}
									}

									if (eBestGiveBonus != NO_BONUS)
									{
										if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (AI_bonusTradeVal(eBestReceiveBonus, ((PlayerTypes)iI), -1) >= GET_PLAYER((PlayerTypes)iI).AI_bonusTradeVal(eBestGiveBonus, getID(), 1)))
										{
											ourList.clear();
											theirList.clear();

											setTradeItem(&item, TRADE_RESOURCES, eBestGiveBonus);
											ourList.insertAtEnd(item);

											setTradeItem(&item, TRADE_RESOURCES, eBestReceiveBonus);
											theirList.insertAtEnd(item);

											if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
											{
												if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
												{
													AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BONUS, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_BONUS));
													pDiplo = new CvDiploParameters(getID());
													FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
													pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
													pDiplo->setAIContact(true);
													pDiplo->setOurOfferList(theirList);
													pDiplo->setTheirOfferList(ourList);
													AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
													abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
												}
											}
											else
											{
												GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
											}
										}
									}
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MAP) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.TradeMaps");

							if (GC.getGame().getSorenRandNum(GC.getLeaderHeadInfo(getPersonalityType()).getContactRand(CONTACT_TRADE_MAP), "AI Diplo Trade Map") == 0)
							{
								setTradeItem(&item, TRADE_MAPS);

								if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true) && canTradeItem(((PlayerTypes)iI), item, true))
								{
									if (!GET_PLAYER((PlayerTypes)iI).isHumanPlayer() || (GET_TEAM(getTeam()).AI_mapTradeVal(GET_PLAYER((PlayerTypes)iI).getTeam()) >= GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_mapTradeVal(getTeam())))
									{
										ourList.clear();
										theirList.clear();

										setTradeItem(&item, TRADE_MAPS);
										ourList.insertAtEnd(item);

										setTradeItem(&item, TRADE_MAPS);
										theirList.insertAtEnd(item);

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
											{
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_MAP, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_MAP));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_OFFER_DEAL"));
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}

						if (AI_getContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BUY_WAR) == 0)
						{
							PROFILE("CvPlayerAI::AI_doDiplo.DeclareWar");

							int iDeclareWarTradeRand = GC.getLeaderHeadInfo(getPersonalityType()).getDeclareWarTradeRand();
							int iMinAtWarCounter = MAX_INT;
							for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
							{
								if (GET_TEAM((TeamTypes)iJ).isAlive())
								{
									if (atWar(((TeamTypes)iJ), getTeam()))
									{
										int iAtWarCounter = GET_TEAM(getTeam()).AI_getAtWarCounter((TeamTypes)iJ);
										if (GET_TEAM(getTeam()).AI_getWarPlan((TeamTypes)iJ) == WARPLAN_DOGPILE)
										{
											iAtWarCounter *= 2;
											iAtWarCounter += 5;
										}
										iMinAtWarCounter = std::min(iAtWarCounter, iMinAtWarCounter);
									}
								}
							}

							logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, iMinAtWarCounter: %d, iDeclareWarTradeRand: %d\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iMinAtWarCounter, iDeclareWarTradeRand);

							if (iMinAtWarCounter < 10)
							{
								iDeclareWarTradeRand *= iMinAtWarCounter;
								iDeclareWarTradeRand /= 10;
								iDeclareWarTradeRand++;
							}

							if (iMinAtWarCounter < 4)
							{
								iDeclareWarTradeRand /= 4;
								iDeclareWarTradeRand++;
							}

							logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, iMinAtWarCounter: %d, adjusted iDeclareWarTradeRand: %d\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iMinAtWarCounter, iDeclareWarTradeRand);

							if (GC.getGame().getSorenRandNum(iDeclareWarTradeRand, "AI Diplo Declare War Trade") == 0)
							{
								iBestValue = 0;
								eBestTeam = NO_TEAM;

								logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally - passed rand check\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName());

								for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
								{
									if (GET_TEAM((TeamTypes)iJ).isAlive()
									&& atWar((TeamTypes)iJ, getTeam())
									&& !atWar((TeamTypes)iJ, GET_PLAYER((PlayerTypes)iI).getTeam())
									&& GET_TEAM((TeamTypes)iJ).getAtWarCount(true) < std::max(2, GC.getGame().countCivTeamsAlive() / 2))
									{
										setTradeItem(&item, TRADE_WAR, iJ);

										if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
										{
											const int iValue =
												(
													(1 + GC.getGame().getSorenRandNum(1000, "AI Declare War Trading"))
													*
													(101 + GET_TEAM((TeamTypes)iJ).AI_getAttitudeWeight(getTeam()))
													/
													100
												);
											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestTeam = ((TeamTypes)iJ);
											}
										}
									}
								}

								if (eBestTeam != NO_TEAM)
									logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, bestValue: %d, best team: %S\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iBestValue, GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
								else logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, no best team found!\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName());

								if (eBestTeam != NO_TEAM)
								{
									iBestValue = 0;
									eBestGiveTech = NO_TECH;

									for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
									{
										setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

										if (canTradeItem((PlayerTypes)iI, item, true))
										{
											const int iValue =
												(
													(1 + GC.getGame().getSorenRandNum(100, "AI Tech Trading #2"))
													*
													GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getResearchLeft((TechTypes)iJ)
												);
											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestGiveTech = (TechTypes)iJ;
											}
										}
									}

									iOurValue = GET_TEAM(getTeam()).AI_declareWarTradeVal(eBestTeam, GET_PLAYER((PlayerTypes)iI).getTeam());
									int iTheirValue;
									if (eBestGiveTech != NO_TECH)
									{
										iTheirValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech, getTeam());
										logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, best tech offer %S at a value of %d\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), GC.getTechInfo((TechTypes)eBestGiveTech).getDescription(), iTheirValue);
									}
									else
									{
										iTheirValue = 0;
										logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, no best tech offer\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName());
									}

									int iBestValue2 = 0;
									TechTypes eBestGiveTech2 = NO_TECH;

									if ((iTheirValue < iOurValue) && (eBestGiveTech != NO_TECH))
									{
										for (int iJ = 0; iJ < GC.getNumTechInfos(); iJ++)
										{
											if (iJ != eBestGiveTech)
											{
												setTradeItem(&item, TRADE_TECHNOLOGIES, iJ);

												if (canTradeItem(((PlayerTypes)iI), item, true))
												{
													const int iValue =
														(
															(1 + GC.getGame().getSorenRandNum(100, "AI Tech Trading #2"))
															*
															GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getResearchLeft((TechTypes)iJ)
														);
													if (iValue > iBestValue)
													{
														iBestValue2 = iValue;
														eBestGiveTech2 = (TechTypes)iJ;
													}
												}
											}
										}

										if (eBestGiveTech2 != NO_TECH)
										{
											int iTechValue = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_techTradeVal(eBestGiveTech2, getTeam());
											iTheirValue += iTechValue;
											logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, 2nd best tech offer %S at a value of %d (total: %d)\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), GC.getTechInfo((TechTypes)eBestGiveTech2).getDescription(), iTechValue, iTheirValue);
										}
									}

									int iReceiveGold = 0;
									int iGiveGold = 0;

									if (iTheirValue > iOurValue)
									{
										const int iValueDiff = iTheirValue - iOurValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = GET_PLAYER((PlayerTypes)iI).AI_maxGoldTrade(getID());

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, asking for %d gold\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iGold);

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (GET_PLAYER((PlayerTypes)iI).canTradeItem(getID(), item, true))
													{
														iReceiveGold = iGold;
														iOurValue += iValue;
													}
												}
											}
										}
									}
									else if (iOurValue > iTheirValue)
									{
										const int iValueDiff = iOurValue - iTheirValue;
										const int iGoldValuePercent = AI_goldTradeValuePercent();
										int iGold = AI_getGoldFromValue(iValueDiff, iGoldValuePercent);
										if (iGold > 0)
										{
											const int iMaxTrade = AI_maxGoldTrade((PlayerTypes)iI);

											if (iGold > iMaxTrade)
											{
												iGold = iMaxTrade;
											}
											else
											{
												// Account for rounding errors
												while (iGold < iMaxTrade && AI_getGoldValue(iGold, iGoldValuePercent) < iValueDiff)
												{
													iGold++;
												}
											}

											logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, offering %d gold\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iGold);

											if (iGold > 0)
											{
												const int iValue = AI_getGoldValue(iGold, iGoldValuePercent);
												if (iValue > 0)
												{
													setTradeItem(&item, TRADE_GOLD, iGold);

													if (canTradeItem(((PlayerTypes)iI), item, true))
													{
														iGiveGold = iGold;
														iTheirValue += iValue;
													}
												}
											}
										}
									}

									logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, iTheirValue: %d, iOurValue: %d\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName(), iTheirValue, iOurValue);

									if (iTheirValue > (iOurValue * 3 / 4))
									{
										logging::logMsg("C2C.log", "%S considering contacting %S to buy a war ally, trade offer proceeding\n", this->getName(), GET_PLAYER((PlayerTypes)iI).getName());

										ourList.clear();
										theirList.clear();

										if (eBestGiveTech != NO_TECH)
										{
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech);
											ourList.insertAtEnd(item);
										}

										if (eBestGiveTech2 != NO_TECH)
										{
											setTradeItem(&item, TRADE_TECHNOLOGIES, eBestGiveTech2);
											ourList.insertAtEnd(item);
										}

										setTradeItem(&item, TRADE_WAR, eBestTeam);
										theirList.insertAtEnd(item);

										if (iGiveGold != 0)
										{
											setTradeItem(&item, TRADE_GOLD, iGiveGold);
											ourList.insertAtEnd(item);
										}

										if (iReceiveGold != 0)
										{
											setTradeItem(&item, TRADE_GOLD, iReceiveGold);
											theirList.insertAtEnd(item);
										}

										if (GET_PLAYER((PlayerTypes)iI).isHumanPlayer())
										{
											if (!(abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()]))
											{
												m_eDemandWarAgainstTeam = eBestTeam;
												AI_changeContactTimer(((PlayerTypes)iI), CONTACT_TRADE_BUY_WAR, GC.getLeaderHeadInfo(getPersonalityType()).getContactDelay(CONTACT_TRADE_BUY_WAR));
												pDiplo = new CvDiploParameters(getID());
												FAssertMsg(pDiplo != NULL, "pDiplo must be valid");
												pDiplo->setDiploComment((DiploCommentTypes)GC.getInfoTypeForString("AI_DIPLOCOMMENT_JOIN_WAR"), GET_PLAYER(GET_TEAM(eBestTeam).getLeaderID()).getCivilizationAdjectiveKey());
												pDiplo->setAIContact(true);
												pDiplo->setOurOfferList(theirList);
												pDiplo->setTheirOfferList(ourList);
												AI_beginDiplomacy(pDiplo, (PlayerTypes)iI);
												abContacted[GET_PLAYER((PlayerTypes)iI).getTeam()] = true;
											}
										}
										else
										{
											m_eDemandWarAgainstTeam = eBestTeam;
											GC.getGame().implementDeal(getID(), ((PlayerTypes)iI), &ourList, &theirList);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}


//
// read object from a stream
// used during load
//
void CvPlayerAI::read(FDataStreamBase* pStream)
{
	PROFILE_EXTRA_FUNC();
	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_READ_OBJECT_START(wrapper);

	CvPlayer::read(pStream); // read base class data first

	uint uiFlag = 0;
	WRAPPER_READ(wrapper, "CvPlayerAI", &uiFlag); // flags for expansion

	if ((uiFlag & PLAYERAI_UI_FLAG_OMITTED) == 0)
	{
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iPeaceWeight);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iEspionageWeight);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iAttackOddsChange);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iCivicTimer);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iReligionTimer);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iExtraGoldTarget);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_turnsSinceLastRevolution);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iCivicSwitchMinDeltaThreshold);

		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyHash);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyHashCacheTurn);

		if (uiFlag < 3)
		{
			m_iStrategyHash = 0;
			m_iStrategyHashCacheTurn = -1;
		}

		if (uiFlag > 2)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iStrategyRand);
		}
		else
		{
			m_iStrategyRand = 0;
		}

		if (uiFlag > 0)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iVictoryStrategyHash);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_iVictoryStrategyHashCacheTurn);
		}
		else
		{
			m_iVictoryStrategyHash = 0;
			m_iVictoryStrategyHashCacheTurn = -1;
		}

		if (uiFlag > 1)
		{
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bPushReligiousVictory);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bConsiderReligiousVictory);
			WRAPPER_READ(wrapper, "CvPlayerAI", &m_bHasInquisitionTarget);
		}
		else
		{
			m_bPushReligiousVictory = false;
			m_bConsiderReligiousVictory = false;
			m_bHasInquisitionTarget = false;
		}
		WRAPPER_READ(wrapper, "CvPlayerAI", (int*)&m_iAveragesCacheTurn);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iAverageGreatPeopleMultiplier);

		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_YIELD_TYPES, m_aiAverageYieldMultiplier);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceMultiplier);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceExchange);

		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCacheTurn);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCachedExpThreshold);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iUpgradeUnitsCachedGold);

		WRAPPER_READ_OPTIONAL_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumTrainAIUnits);
		WRAPPER_READ_OPTIONAL_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumAIUnits);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiSameReligionCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiDifferentReligionCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiFavoriteCivicCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiBonusTradeCounter);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeTradeValue);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeGrantValue);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiGoldTradedTo);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiAttitudeExtra);

		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_abFirstContact);

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", NUM_CONTACT_TYPES, m_aaiContactTimer[i]);
		}
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_READ_ARRAY_ALLOW_TRUNCATE(wrapper, "CvPlayerAI", NUM_MEMORY_TYPES, m_aaiMemoryCount[i]);
		}
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_bWasFinancialTrouble);
		WRAPPER_READ(wrapper, "CvPlayerAI", &m_iTurnLastProductionDirty);
		{
			m_aiAICitySites.clear();
			uint iSize;
			WRAPPER_READ(wrapper, "CvPlayerAI", &iSize);
			for (uint i = 0; i < iSize; i++)
			{
				int iCitySite;
				WRAPPER_READ(wrapper, "CvPlayerAI", &iCitySite);
				m_aiAICitySites.push_back(iCitySite);
			}
		}
		WRAPPER_READ_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_aiUnitWeights);
		WRAPPER_READ_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_aiUnitCombatWeights);
		WRAPPER_READ_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiCloseBordersAttitudeCache);

		m_eBestResearchTarget = NO_TECH;
		WRAPPER_READ_CLASS_ENUM(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_TECHS, (int*)&m_eBestResearchTarget);

		AI_updateBonusValue();
	}
	WRAPPER_READ_OBJECT_END(wrapper);

	//	If the total number of barb units is getting dangerously close to the limit we can
	//	cull some animals.  Note - this has to be done in PlayerAI not Player, because kills won't
	//	operate correctly until AI structures are initialized (class counting for example)
	//  TB: Adjusted to cull any NPC units as needed.
	if (isNPC() && getNumUnits() > MAX_BARB_UNITS_FOR_SPAWNING)
	{
		int iCull = getNumUnits() - MAX_BARB_UNITS_FOR_SPAWNING;

		std::vector<CvUnit*> unitsToCull(beginUnits(), endUnits());
		unitsToCull.resize(std::min<size_t>(iCull, unitsToCull.size()));

		foreach_(CvUnit * unit, unitsToCull)
		{
			unit->kill(false);
		}
	}
	AI_invalidateAttitudeCache();
}


//
// save object to a stream
// used during save
//
void CvPlayerAI::write(FDataStreamBase* pStream)
{
	PROFILE_FUNC();

	CvTaggedSaveFormatWrapper& wrapper = CvTaggedSaveFormatWrapper::getSaveFormatWrapper();

	wrapper.AttachToStream(pStream);

	WRAPPER_WRITE_OBJECT_START(wrapper);

	CvPlayer::write(pStream); // write base class data first

	// Flag for type of save
	uint uiFlag = 3;
	if (!m_bEverAlive)
	{
		uiFlag |= PLAYERAI_UI_FLAG_OMITTED;
	}

	WRAPPER_WRITE(wrapper, "CvPlayerAI", uiFlag); // flag for expansion

	if (m_bEverAlive)
	{
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iPeaceWeight);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iEspionageWeight);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAttackOddsChange);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iCivicTimer);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iReligionTimer);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iExtraGoldTarget);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_turnsSinceLastRevolution);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iCivicSwitchMinDeltaThreshold);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyHash);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyHashCacheTurn);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iStrategyRand);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iVictoryStrategyHash);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iVictoryStrategyHashCacheTurn);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bPushReligiousVictory);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bConsiderReligiousVictory);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bHasInquisitionTarget);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAveragesCacheTurn);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iAverageGreatPeopleMultiplier);

		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_YIELD_TYPES, m_aiAverageYieldMultiplier);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceMultiplier);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_COMMERCE_TYPES, m_aiAverageCommerceExchange);

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCacheTurn);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCachedExpThreshold);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iUpgradeUnitsCachedGold);

		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumTrainAIUnits);
		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITAIS, NUM_UNITAI_TYPES, m_aiNumAIUnits);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiSameReligionCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiDifferentReligionCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiFavoriteCivicCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiBonusTradeCounter);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeTradeValue);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiPeacetimeGrantValue);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiGoldTradedTo);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiAttitudeExtra);

		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_abFirstContact);

		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_CONTACT_TYPES, m_aaiContactTimer[i]);
		}
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", NUM_MEMORY_TYPES, m_aaiMemoryCount[i]);
		}

		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_bWasFinancialTrouble);
		WRAPPER_WRITE(wrapper, "CvPlayerAI", m_iTurnLastProductionDirty);

		{
			const uint32_t iSize = m_aiAICitySites.size();
			WRAPPER_WRITE(wrapper, "CvPlayerAI", iSize);
			foreach_(const int iCitySite, m_aiAICitySites)
			{
				WRAPPER_WRITE_DECORATED(wrapper, "CvPlayerAI", iCitySite, "iCitySite");
			}
		}

		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_UNITS, GC.getNumUnitInfos(), m_aiUnitWeights);
		WRAPPER_WRITE_CLASS_ARRAY(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_COMBATINFOS, GC.getNumUnitCombatInfos(), m_aiUnitCombatWeights);
		WRAPPER_WRITE_ARRAY(wrapper, "CvPlayerAI", MAX_PLAYERS, m_aiCloseBordersAttitudeCache);

		WRAPPER_WRITE_CLASS_ENUM(wrapper, "CvPlayerAI", REMAPPED_CLASS_TYPE_TECHS, m_eBestResearchTarget);
	}
	WRAPPER_WRITE_OBJECT_END(wrapper);

	/* Needed if getMaxCivPlayers return MAX_PC_PLAYERS, now it returns MAX_PLAYERS-1.
		if (getID() == MAX_PC_PLAYERS)
		{
			//Write NPC data
			for (int iI = MAX_PC_PLAYERS+1; iI < MAX_PLAYERS; iI++)
			{
				GET_PLAYER((PlayerTypes)iI).write(pStream);
			}
		}
	*/
}


int CvPlayerAI::AI_eventValue(EventTypes eEvent, const EventTriggeredData& kTriggeredData) const
{
	PROFILE_EXTRA_FUNC();
	if (kTriggeredData.m_eTrigger == NO_EVENTTRIGGER)
	{
		return 0;
	}
	const CvEventInfo& kEvent = GC.getEventInfo(eEvent);

	int iNumCities = getNumCities();
	CvCity* pCity = getCity(kTriggeredData.m_iCityId);
	CvPlot* pPlot = GC.getMap().plot(kTriggeredData.m_iPlotX, kTriggeredData.m_iPlotY);
	CvUnit* pUnit = getUnit(kTriggeredData.m_iUnitId);

	int iHappy = 0;
	int iHealth = 0;
	int aiYields[NUM_YIELD_TYPES];
	int aiCommerceYields[NUM_COMMERCE_TYPES];

	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		aiYields[iI] = 0;
	}
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		aiCommerceYields[iI] = 0;
	}

	if (NO_PLAYER != kTriggeredData.m_eOtherPlayer)
	{
		if (kEvent.isDeclareWar())
		{
			switch (AI_getAttitude(kTriggeredData.m_eOtherPlayer))
			{
			case ATTITUDE_FURIOUS:
			case ATTITUDE_ANNOYED:
			case ATTITUDE_CAUTIOUS:
				if (GET_TEAM(getTeam()).getDefensivePower() < GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).getPower(true))
				{
					return -MAX_INT + 1;
				}
				break;
			case ATTITUDE_PLEASED:
			case ATTITUDE_FRIENDLY:
				return -MAX_INT + 1;
				break;
			}
		}
	}

	//Proportional to #turns in the game...
	//(AI evaluation will generally assume proper game speed scaling!)
	const int iGameSpeedPercent = GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent();

	int iValue = GC.getGame().getSorenRandNum(kEvent.getAIValue(), "AI Event choice");
	iValue += (getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, false) + getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, true)) / 2;

	iValue += kEvent.getEspionagePoints();

	if (kEvent.getTech() != NO_TECH)
	{
		iValue += (GET_TEAM(getTeam()).getResearchCost((TechTypes)kEvent.getTech()) * kEvent.getTechPercent()) / 100;
	}

	if (kEvent.getFreeUnit() != NO_UNIT)
	{
		//Altough AI_unitValue compares well within units, the value is somewhat independent of cost
		int iUnitValue = GC.getUnitInfo((UnitTypes)kEvent.getFreeUnit()).getProductionCost();
		if (iUnitValue > 0)
		{
			iUnitValue *= 2;
		}
		else if (iUnitValue == -1)
		{
			iUnitValue = 200; //Great Person?
		}

		iUnitValue *= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent();
		iValue += kEvent.getNumUnits() * iUnitValue;
	}

	if (kEvent.isDisbandUnit())
	{
		CvUnit* pUnit = getUnit(kTriggeredData.m_iUnitId);
		if (NULL != pUnit)
		{
			int iUnitValue = pUnit->getUnitInfo().getProductionCost();
			if (iUnitValue > 0)
			{
				iUnitValue *= 2;
			}
			else if (iUnitValue == -1)
			{
				iUnitValue = 200; //Great Person?
			}

			iUnitValue *= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent();
			iValue -= iUnitValue;
		}
	}

	const BuildingTypes eBuilding = static_cast<BuildingTypes>(kEvent.getBuilding());
	if (eBuilding != NO_BUILDING && pCity)
	{
		int iBuildingValue = GC.getBuildingInfo(eBuilding).getProductionCost();
		if (iBuildingValue > 0)
		{
			iBuildingValue *= 2;
		}
		else if (iBuildingValue == -1)
		{
			iBuildingValue = 300; //Shrine?
		}

		iBuildingValue *= GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent();
		iValue += kEvent.getBuildingChange() * iBuildingValue;
	}

	TechTypes eBestTech = NO_TECH;
	int iBestValue = 0;
	for (int iTech = 0; iTech < GC.getNumTechInfos(); ++iTech)
	{
		if (canResearch((TechTypes)iTech))
		{
			if (NO_PLAYER == kTriggeredData.m_eOtherPlayer || GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).isHasTech((TechTypes)iTech))
			{
				int iValue = 0;
				for (int i = 0; i < GC.getNumFlavorTypes(); ++i)
				{
					iValue += kEvent.getTechFlavorValue(i) * GC.getTechInfo((TechTypes)iTech).getFlavorValue(i);
				}

				if (iValue > iBestValue)
				{
					eBestTech = (TechTypes)iTech;
					iBestValue = iValue;
				}
			}
		}
	}

	if (eBestTech != NO_TECH)
	{
		iValue += (GET_TEAM(getTeam()).getResearchCost(eBestTech) * kEvent.getTechPercent()) / 100;
	}

	if (kEvent.isGoldenAge())
	{
		iValue += AI_calculateGoldenAgeValue();
	}

	{	//Yield and other changes
		if (kEvent.getNumBuildingYieldChanges() > 0)
		{
			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
			{
				for (int iYield = 0; iYield < NUM_YIELD_TYPES; ++iYield)
				{
					aiYields[iYield] += kEvent.getBuildingYieldChange(iBuilding, iYield);
				}
			}
		}

		if (kEvent.getNumBuildingCommerceChanges() > 0)
		{
			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
			{
				for (int iCommerce = 0; iCommerce < NUM_COMMERCE_TYPES; ++iCommerce)
				{
					aiCommerceYields[iCommerce] += kEvent.getBuildingCommerceChange(iBuilding, iCommerce);
				}
			}
		}

		if (kEvent.getNumBuildingHappyChanges() > 0)
		{
			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
			{
				iHappy += kEvent.getBuildingHappyChange(iBuilding);
			}
		}

		if (kEvent.getNumBuildingHealthChanges() > 0)
		{
			for (int iBuilding = 0; iBuilding < GC.getNumBuildingInfos(); ++iBuilding)
			{
				iHealth += kEvent.getBuildingHealthChange(iBuilding);
			}
		}
	}

	if (kEvent.isCityEffect())
	{
		int iCityPopulation = -1;
		int iCityTurnValue = 0;
		if (NULL != pCity)
		{
			iCityPopulation = pCity->getPopulation();
			for (int iSpecialist = 0; iSpecialist < GC.getNumSpecialistInfos(); ++iSpecialist)
			{
				if (kEvent.getFreeSpecialistCount(iSpecialist) > 0)
				{
					iCityTurnValue += (pCity->AI_specialistValue((SpecialistTypes)iSpecialist, false, false) / 300);
				}
			}
		}

		if (-1 == iCityPopulation)
		{
			//What is going on here?
			iCityPopulation = 5;
		}

		iCityTurnValue += ((iHappy + kEvent.getHappy()) * 8);
		iCityTurnValue += ((iHealth + kEvent.getHealth()) * 6);

		iCityTurnValue += aiYields[YIELD_FOOD] * 5;
		iCityTurnValue += aiYields[YIELD_PRODUCTION] * 5;
		iCityTurnValue += aiYields[YIELD_COMMERCE] * 3;

		iCityTurnValue += aiCommerceYields[COMMERCE_RESEARCH] * 3;
		iCityTurnValue += aiCommerceYields[COMMERCE_GOLD] * 3;
		iCityTurnValue += aiCommerceYields[COMMERCE_CULTURE] * 1;
		iCityTurnValue += aiCommerceYields[COMMERCE_ESPIONAGE] * 2;

		iValue += iCityTurnValue * 20 * iGameSpeedPercent / 100;

		iValue += kEvent.getFood();
		iValue += kEvent.getFoodPercent() / 4;
		iValue += kEvent.getPopulationChange() * 30;
		iValue -= kEvent.getRevoltTurns() * (12 + iCityPopulation * 16);
		iValue -= kEvent.getHurryAnger() * 6 * GC.getHURRY_ANGER_DIVISOR() * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / 100;
		iValue += kEvent.getHappyTurns() * 10;
		iValue += kEvent.getCulture() / 2;
	}
	else if (!kEvent.isOtherPlayerCityEffect())
	{
		const int iPerTurnValue = iNumCities * (iHappy * 4 + kEvent.getHappy() * 8 + iHealth * 3 + kEvent.getHealth() * 6);

		iValue += iPerTurnValue * 20 * iGameSpeedPercent / 100;

		iValue += (kEvent.getFood() * iNumCities);
		iValue += (kEvent.getFoodPercent() * iNumCities) / 4;
		iValue += (kEvent.getPopulationChange() * iNumCities * 40);
		iValue -= iNumCities * kEvent.getHurryAnger() * 6 * GC.getHURRY_ANGER_DIVISOR() * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent() / 100;
		iValue += iNumCities * kEvent.getHappyTurns() * 10;
		iValue += iNumCities * kEvent.getCulture() / 2;
	}

	int iBonusValue = 0;
	if (NO_BONUS != kEvent.getBonus())
	{
		iBonusValue = AI_bonusVal((BonusTypes)kEvent.getBonus());
	}

	if (NULL != pPlot)
	{
		if (kEvent.getFeatureChange() != 0)
		{
			int iOldFeatureValue = 0;
			int iNewFeatureValue = 0;
			if (pPlot->getFeatureType() != NO_FEATURE)
			{
				//*grumble* who tied feature production to builds rather than the feature...
				iOldFeatureValue = GC.getFeatureInfo(pPlot->getFeatureType()).getHealthPercent();

				if (kEvent.getFeatureChange() > 0)
				{
					FeatureTypes eFeature = (FeatureTypes)kEvent.getFeature();
					FAssert(eFeature != NO_FEATURE);
					if (eFeature != NO_FEATURE)
					{
						iNewFeatureValue = GC.getFeatureInfo(eFeature).getHealthPercent();
					}
				}

				iValue += ((iNewFeatureValue - iOldFeatureValue) * iGameSpeedPercent) / 100;
			}
		}

		if (kEvent.getImprovementChange() > 0)
		{
			iValue += (30 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getImprovementChange() < 0)
		{
			iValue -= (30 * iGameSpeedPercent) / 100;
		}

		if (kEvent.getRouteChange() > 0)
		{
			iValue += (10 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getRouteChange() < 0)
		{
			iValue -= (10 * iGameSpeedPercent) / 100;
		}

		if (kEvent.getBonusChange() > 0)
		{
			iValue += (iBonusValue * 15 * iGameSpeedPercent) / 100;
		}
		else if (kEvent.getBonusChange() < 0)
		{
			iValue -= (iBonusValue * 15 * iGameSpeedPercent) / 100;
		}

		for (int i = 0; i < NUM_YIELD_TYPES; ++i)
		{
			if (0 != kEvent.getPlotExtraYield(i))
			{
				if (pPlot->getWorkingCity() != NULL)
				{
					FAssertMsg(pPlot->getWorkingCity()->getOwner() == getID(), "Event creates a boni for another player?");
					aiYields[i] += kEvent.getPlotExtraYield(i);
				}
				else
				{
					iValue += (20 * 8 * kEvent.getPlotExtraYield(i) * iGameSpeedPercent) / 100;
				}
			}
		}
	}

	if (NO_BONUS != kEvent.getBonusRevealed())
	{
		iValue += (iBonusValue * 10 * iGameSpeedPercent) / 100;
	}

	if (NULL != pUnit)
	{
		iValue += (2 * pUnit->baseCombatStrNonGranular() * kEvent.getUnitExperience() * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent()) / 100;

		iValue -= 10 * kEvent.getUnitImmobileTurns();
	}

	{
		int iPromotionValue = 0;

		for (int i = 0; i < GC.getNumUnitCombatInfos(); ++i)
		{
			if (NO_PROMOTION != kEvent.getUnitCombatPromotion(i))
			{
				foreach_(const CvUnit * pLoopUnit, units())
				{
					//if (pLoopUnit->getUnitCombatType() == i)
					//{
					//	if (!pLoopUnit->isHasPromotion((PromotionTypes)kEvent.getUnitCombatPromotion(i)))
					//	{
					//		iPromotionValue += 5 * pLoopUnit->baseCombatStr();
					//	}
					//}
					//TB SubCombat Mod Begin
					if (pLoopUnit->isHasUnitCombat((UnitCombatTypes)i))
					{
						if (!pLoopUnit->isHasPromotion((PromotionTypes)kEvent.getUnitCombatPromotion(i)))
						{
							iPromotionValue += 5 * pLoopUnit->baseCombatStrNonGranular();
						}
					}
					//TB SubCombat Mod End
				}

				iPromotionValue += iNumCities * 50;
			}
		}

		iValue += (iPromotionValue * iGameSpeedPercent) / 100;
	}

	if (kEvent.getFreeUnitSupport() != 0)
	{
		iValue += (20 * kEvent.getFreeUnitSupport() * iGameSpeedPercent) / 100;
	}

	if (kEvent.getInflationModifier() != 0)
	{
		iValue -= static_cast<int>(20 * kEvent.getInflationModifier() * calculatePreInflatedCosts() * iGameSpeedPercent / 100);
	}

	if (kEvent.getSpaceProductionModifier() != 0)
	{
		iValue += ((20 + iNumCities) * getSpaceProductionModifier() * iGameSpeedPercent) / 100;
	}

	int iOtherPlayerAttitudeWeight = 0;
	if (kTriggeredData.m_eOtherPlayer != NO_PLAYER)
	{
		iOtherPlayerAttitudeWeight = AI_getAttitudeWeight(kTriggeredData.m_eOtherPlayer);
		iOtherPlayerAttitudeWeight += 10 - GC.getGame().getSorenRandNum(20, "AI event value attitude");
	}

	//Religion
	if (kTriggeredData.m_eReligion != NO_RELIGION)
	{
		ReligionTypes eReligion = kTriggeredData.m_eReligion;

		int iReligionValue = 15;

		if (getStateReligion() == eReligion)
		{
			iReligionValue += 15;
		}
		if (hasHolyCity(eReligion))
		{
			iReligionValue += 15;
		}

		iValue += (kEvent.getConvertOwnCities() * iReligionValue * iGameSpeedPercent) / 100;

		if (kEvent.getConvertOtherCities() > 0)
		{
			//Don't like them much = fairly indifferent, hate them = negative.
			iValue += (kEvent.getConvertOtherCities() * (iOtherPlayerAttitudeWeight + 50) * iReligionValue * iGameSpeedPercent) / 15000;
		}
	}

	if (NO_PLAYER != kTriggeredData.m_eOtherPlayer)
	{
		CvPlayerAI& kOtherPlayer = GET_PLAYER(kTriggeredData.m_eOtherPlayer);

		int iDiploValue = 0;
		//if we like this player then value positive attitude, if however we really hate them then
		//actually value negative attitude.
		iDiploValue += ((iOtherPlayerAttitudeWeight + 50) * kEvent.getAttitudeModifier() * GET_PLAYER(kTriggeredData.m_eOtherPlayer).getPower()) / std::max(1, getPower());

		if (kEvent.getTheirEnemyAttitudeModifier() != 0)
		{
			//Oh wow this sure is mildly complicated.
			TeamTypes eWorstEnemy = GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).AI_getWorstEnemy();

			if (NO_TEAM != eWorstEnemy && eWorstEnemy != getTeam())
			{
				int iThirdPartyAttitudeWeight = GET_TEAM(getTeam()).AI_getAttitudeWeight(eWorstEnemy);

				//If we like both teams, we want them to get along.
				//If we like otherPlayer but not enemy (or vice-verca), we don't want them to get along.
				//If we don't like either, we don't want them to get along.
				//Also just value stirring up trouble in general.

				int iThirdPartyDiploValue = 50 * kEvent.getTheirEnemyAttitudeModifier();
				iThirdPartyDiploValue *= (iThirdPartyAttitudeWeight - 10);
				iThirdPartyDiploValue *= (iOtherPlayerAttitudeWeight - 10);
				iThirdPartyDiploValue /= 10000;

				if ((iOtherPlayerAttitudeWeight) < 0 && (iThirdPartyAttitudeWeight < 0))
				{
					iThirdPartyDiploValue *= -1;
				}

				iThirdPartyDiploValue /= 2;

				iDiploValue += iThirdPartyDiploValue;
			}
		}

		iDiploValue *= iGameSpeedPercent;
		iDiploValue /= 100;

		if (NO_BONUS != kEvent.getBonusGift())
		{
			int iBonusValue = -AI_bonusVal((BonusTypes)kEvent.getBonusGift(), -1);
			iBonusValue += (iOtherPlayerAttitudeWeight - 40) * kOtherPlayer.AI_bonusVal((BonusTypes)kEvent.getBonusGift(), +1);
			//Positive for friends, negative for enemies.
			iDiploValue += (iBonusValue * getTreatyLength()) / 60;
		}

		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			//What is this "relationships" thing?
			iDiploValue /= 2;
		}

		if (kEvent.isGoldToPlayer())
		{
			//If the gold goes to another player instead of the void, then this is a positive
			//thing if we like the player, otherwise it's a negative thing.
			int iGiftValue = (getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, false) + getEventCost(eEvent, kTriggeredData.m_eOtherPlayer, true)) / 2;
			iGiftValue *= -iOtherPlayerAttitudeWeight;
			iGiftValue /= 110;

			iValue += iGiftValue;
		}

		if (kEvent.isDeclareWar())
		{
			int iWarValue = (GET_TEAM(getTeam()).getDefensivePower() - GET_TEAM(GET_PLAYER(kTriggeredData.m_eOtherPlayer).getTeam()).getPower(true));// / std::max(1, GET_TEAM(getTeam()).getDefensivePower());
			iWarValue -= 30 * AI_getAttitudeVal(kTriggeredData.m_eOtherPlayer);
		}

		if (kEvent.getMaxPillage() > 0)
		{
			int iPillageValue = (40 * (kEvent.getMinPillage() + kEvent.getMaxPillage())) / 2;
			//If we hate them, this is good to do.
			iPillageValue *= 25 - iOtherPlayerAttitudeWeight;
			iPillageValue *= iGameSpeedPercent;
			iPillageValue /= 12500;
		}

		iValue += (iDiploValue * iGameSpeedPercent) / 100;
	}

	int iThisEventValue = iValue;
	//XXX THIS IS VULNERABLE TO NON-TRIVIAL RECURSIONS!
	//Event A effects Event B, Event B effects Event A
	for (int iEvent = 0; iEvent < GC.getNumEventInfos(); ++iEvent)
	{
		if (kEvent.getAdditionalEventChance(iEvent) > 0)
		{
			if (iEvent == eEvent)
			{
				//Infinite recursion is not our friend.
				//Fortunately we have the event value for this event - sans values of other events
				//disabled or cleared. Hopefully no events will be that complicated...
				//Double the value since it's recursive.
				iValue += (kEvent.getAdditionalEventChance(iEvent) * iThisEventValue) / 50;
			}
			else
			{
				iValue += (kEvent.getAdditionalEventChance(iEvent) * AI_eventValue((EventTypes)iEvent, kTriggeredData)) / 100;
			}
		}

		if (kEvent.getClearEventChance(iEvent) > 0)
		{
			if (iEvent == eEvent)
			{
				iValue -= (kEvent.getClearEventChance(iEvent) * iThisEventValue) / 50;
			}
			else
			{
				iValue -= (kEvent.getClearEventChance(iEvent) * AI_eventValue((EventTypes)iEvent, kTriggeredData)) / 100;
			}
		}
	}

	iValue *= 100 + GC.getGame().getSorenRandNum(20, "AI Event choice");
	iValue /= 100;

	return iValue;
}

EventTypes CvPlayerAI::AI_chooseEvent(int iTriggeredId, int* pValue) const
{
	PROFILE_EXTRA_FUNC();
	const EventTriggeredData* pTriggeredData = getEventTriggered(iTriggeredId);
	if (NULL == pTriggeredData || pTriggeredData->m_eTrigger == NO_EVENTTRIGGER)
	{
		return NO_EVENT;
	}

	const CvEventTriggerInfo& kTrigger = GC.getEventTriggerInfo(pTriggeredData->m_eTrigger);

	int iBestValue = -MAX_INT;
	EventTypes eBestEvent = NO_EVENT;

	for (int i = 0; i < kTrigger.getNumEvents(); i++)
	{
		int iValue = -MAX_INT;
		if (kTrigger.getEvent(i) != NO_EVENT)
		{
			if (canDoEvent((EventTypes)kTrigger.getEvent(i), *pTriggeredData))
			{
				iValue = AI_eventValue((EventTypes)kTrigger.getEvent(i), *pTriggeredData);
			}
		}

		if (iValue > iBestValue)
		{
			iBestValue = iValue;
			eBestEvent = (EventTypes)kTrigger.getEvent(i);
		}
	}

	if (pValue != NULL)
	{
		*pValue = iBestValue;
	}

	return eBestEvent;
}

void CvPlayerAI::AI_doSplit()
{
	PROFILE_FUNC();

	if (!canSplitEmpire())
	{
		return;
	}
	// Toffer - ToDo
	//	Need to stop AI from creating colony on another landmass when it should rather relocate its capitol to the other landmass.
	//	e.g. Its capitol is a lone city on an island, and it has multiple cities on a bigger landmass, it should relocate its capitol
	//	rather than making a vassal of all the cities on the big landmass, need to look into AI evaluation for palace construction too.
	//	hint: City->AI_cityValue(bIgnoreColonyMaintenance), (new argument) could be used to see if the oversea area is a better area for the capitol.
	//		looped over all cities in each area similar to what is done below.
	//		Need a should relocate capitol function called from here to stop the split if true,
	//		but also from wherever it is natural to evaluate whether the capitol should be relocated or not.
	//		Palace should probably be relocated to the other landmass long before the AI considers splitting it up as a colony (this function is called).
	//		not sure if Forbidden Palace and similar government centers removes oversea maintenance cost from landmass it is on, needs to be looked into.

	std::map<int, int> mapAreaValues;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		mapAreaValues[pLoopArea->getID()] = 0;
	}

	foreach_(CvCity * pLoopCity, cities())
	{
		mapAreaValues[pLoopCity->area()->getID()] += pLoopCity->AI_cityValue();
	}

	std::map<int, int>::iterator it;
	for (it = mapAreaValues.begin(); it != mapAreaValues.end(); ++it)
	{
		if (it->second < 0)
		{
			const int iAreaId = it->first;

			if (canSplitArea(iAreaId))
			{
				splitEmpire(iAreaId);

				foreach_(CvUnit * pUnit, units())
				{
					if (pUnit->area()->getID() == iAreaId)
					{
						const TeamTypes ePlotTeam = pUnit->plot()->getTeam();

						if (NO_TEAM != ePlotTeam)
						{
							const CvTeam& kPlotTeam = GET_TEAM(ePlotTeam);
							if (kPlotTeam.isVassal(getTeam()) && GET_TEAM(getTeam()).isParent(ePlotTeam))
							{
								pUnit->gift();
							}
						}
					}
				}
				break;
			}
		}
	}
}

void CvPlayerAI::AI_launch(VictoryTypes eVictory)
{
	PROFILE_EXTRA_FUNC();
	if (GET_TEAM(getTeam()).isHuman())
	{
		return;
	}

	if (!GET_TEAM(getTeam()).canLaunch(eVictory))
	{
		return;
	}

	bool bLaunch = true;

	int iBestArrival = MAX_INT;
	TeamTypes eBestTeam = NO_TEAM;

	for (int iTeam = 0; iTeam < MAX_PC_TEAMS; ++iTeam)
	{
		if (iTeam != getTeam())
		{
			CvTeam& kTeam = GET_TEAM((TeamTypes)iTeam);
			if (kTeam.isAlive())
			{
				const int iCountdown = kTeam.getVictoryCountdown(eVictory);
				if (iCountdown > 0 && iCountdown < iBestArrival)
				{
					iBestArrival = iCountdown;
					eBestTeam = (TeamTypes)iTeam;
				}
			}
		}
	}

	if (bLaunch)
	{
		if (NO_TEAM == eBestTeam || iBestArrival > GET_TEAM(getTeam()).getVictoryDelay(eVictory))
		{
			if (GET_TEAM(getTeam()).getLaunchSuccessRate(eVictory) < 100)
			{
				bLaunch = false;
			}
		}
	}

	if (bLaunch)
	{
		launch(eVictory);
	}
}

void CvPlayerAI::AI_doCheckFinancialTrouble()
{
	PROFILE_FUNC();

	// if we just ran into financial trouble this turn
	bool bFinancialTrouble = AI_isFinancialTrouble();
	if (bFinancialTrouble != m_bWasFinancialTrouble)
	{
		if (bFinancialTrouble)
		{
			int iGameTurn = GC.getGame().getGameTurn();

			// only reset at most every 10 turns
			if (iGameTurn > m_iTurnLastProductionDirty + 10)
			{
				// redeterimine the best things to build in each city
				AI_makeProductionDirty();

				m_iTurnLastProductionDirty = iGameTurn;
			}
		}

		m_bWasFinancialTrouble = bFinancialTrouble;
	}
}

bool CvPlayerAI::AI_disbandUnit(int iExpThreshold)
{
	PROFILE_EXTRA_FUNC();
	int iBestValue = MAX_INT;
	CvUnit* pBestUnit = NULL;

	foreach_(CvUnit * unitX, units())
	{
		if (unitX->getUpkeep100() < 1
		|| !unitX->isMilitaryBranch()
		|| unitX->hasCargo()
		|| unitX->isGoldenAge()
		|| unitX->getUnitInfo().getProductionCost() < 1)
		{
			continue;
		}
		if ((iExpThreshold == -1 || unitX->canFight() && unitX->getExperience() <= iExpThreshold)
		&& (!unitX->isMilitaryHappiness() || !unitX->plot()->isCity() || unitX->plot()->plotCount(PUF_isMilitaryHappiness, -1, -1, NULL, getID()) > 2))
		{
			int iValue = (10000 + GC.getGame().getSorenRandNum(1000, "Disband Unit"));

			iValue *= 100 + (unitX->getUnitInfo().getProductionCost() * 3);
			iValue /= 100;

			iValue *= 100 + (unitX->getExperience() * 10);
			iValue /= 100;

			iValue *= 100 + (unitX->getLevel() * 25);
			iValue /= 100;

			if (unitX->plot()->getTeam() == unitX->getTeam())
			{
				iValue *= 3;

				if (unitX->canDefend() && unitX->plot()->isCity())
				{
					iValue *= 2;
				}
			}

			// Multiplying by higher number means unit has higher priority, less likely to be disbanded
			switch (unitX->AI_getUnitAIType())
			{
			case UNITAI_UNKNOWN:
			case UNITAI_ANIMAL:
			case UNITAI_BARB_CRIMINAL:
				break;

			case UNITAI_SUBDUED_ANIMAL:
				//	For now make them less valuable the more you have (strictly this should depend
				//	on what you need in terms of their buildable buildings, but start with an
				//	approximation that is better than nothing
				iValue *= std::min(0, getNumCities() * 2 - AI_getNumAIUnits(UNITAI_SUBDUED_ANIMAL)) / std::min(1, getNumCities());
				break;

			case UNITAI_HUNTER:
			case UNITAI_HUNTER_ESCORT:
				//	Treat hunters like explorers for valuation, but slightly less so
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 10;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_SETTLE:
				iValue *= 20;
				break;

			case UNITAI_WORKER:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) > 10)
				{
					if (unitX->plot()->isCity())
					{
						if (unitX->plot()->getPlotCity()->AI_getWorkersNeeded() == 0)
						{
							iValue *= 10;
						}
					}
				}
				break;

			case UNITAI_ATTACK:
			case UNITAI_ATTACK_CITY:
			case UNITAI_COLLATERAL:
			case UNITAI_PILLAGE:
			case UNITAI_RESERVE:
			case UNITAI_COUNTER:
			case UNITAI_PILLAGE_COUNTER:
			case UNITAI_INVESTIGATOR:
			case UNITAI_SEE_INVISIBLE:
				iValue *= 2;
				break;

			case UNITAI_SEE_INVISIBLE_SEA:
				iValue *= 3;
				break;

			case UNITAI_CITY_DEFENSE:
			case UNITAI_CITY_COUNTER:
			case UNITAI_CITY_SPECIAL:
			case UNITAI_PARADROP:
			case UNITAI_PROPERTY_CONTROL:
			case UNITAI_HEALER:
			case UNITAI_PROPERTY_CONTROL_SEA:
			case UNITAI_HEALER_SEA:
			case UNITAI_ESCORT:
				iValue *= 6;
				break;

			case UNITAI_EXPLORE:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 15;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_MISSIONARY:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 8;
				}
				break;

			case UNITAI_PROPHET:
			case UNITAI_ARTIST:
			case UNITAI_SCIENTIST:
			case UNITAI_GENERAL:
			case UNITAI_GREAT_HUNTER:
			case UNITAI_GREAT_ADMIRAL:
			case UNITAI_MERCHANT:
			case UNITAI_ENGINEER:
				iValue *= 20;
				break;

			case UNITAI_SPY:
			case UNITAI_INFILTRATOR:
				iValue *= 12;
				break;

			case UNITAI_ICBM:
				iValue *= 4;
				break;

			case UNITAI_WORKER_SEA:
				iValue *= 18;
				break;

			case UNITAI_ATTACK_SEA:
			case UNITAI_RESERVE_SEA:
			case UNITAI_ESCORT_SEA:
				break;

			case UNITAI_EXPLORE_SEA:
				if ((GC.getGame().getGameTurn() - unitX->getGameTurnCreated()) < 10
					|| unitX->plot()->getTeam() != getTeam())
				{
					iValue *= 12;
				}
				break;

			case UNITAI_SETTLER_SEA:
				iValue *= 6;
				break;

			case UNITAI_MISSIONARY_SEA:
			case UNITAI_SPY_SEA:
				iValue *= 4;
				break;

			case UNITAI_ASSAULT_SEA:
			case UNITAI_CARRIER_SEA:
			case UNITAI_MISSILE_CARRIER_SEA:
				if (GET_TEAM(getTeam()).hasWarPlan(true))
				{
					iValue *= 5;
				}
				else
				{
					iValue *= 2;
				}
				break;

			case UNITAI_PIRATE_SEA:
			case UNITAI_ATTACK_AIR:
				break;

			case UNITAI_DEFENSE_AIR:
			case UNITAI_CARRIER_AIR:
			case UNITAI_MISSILE_AIR:
				if (GET_TEAM(getTeam()).hasWarPlan(true))
				{
					iValue *= 5;
				}
				else
				{
					iValue *= 3;
				}
				break;

			default:
				FErrorMsg("error");
				break;
			}

			iValue /= unitX->getUnitInfo().getBaseUpkeep() + 1;

			if (iValue < iBestValue)
			{
				iBestValue = iValue;
				pBestUnit = unitX;
			}
		}
	}

	if (pBestUnit)
	{
		if (gPlayerLogLevel >= 2)
		{
			CvWString szString;
			getUnitAIString(szString, pBestUnit->AI_getUnitAIType());

			logBBAI("	Player %d (%S) disbanding %S with UNITAI %S to save cash", getID(), getCivilizationDescription(0), pBestUnit->getName().GetCString(), szString.GetCString());
		}
		pBestUnit->kill(false);
		return true;
	}
	return false;
}

int CvPlayerAI::AI_cultureVictoryTechValue(TechTypes eTech) const
{

	PROFILE_EXTRA_FUNC();
	if (eTech == NO_TECH)
	{
		return 0;
	}
	int iValue = 0;

	if (GC.getTechInfo(eTech).isDefensivePactTrading())
	{
		iValue += 50;
	}

	if (GC.getTechInfo(eTech).isCommerceFlexible(COMMERCE_CULTURE))
	{
		iValue += 100;
	}

	//units
	const bool bAnyWarplan = GET_TEAM(getTeam()).hasWarPlan(true);
	int iBestUnitValue = 0;
	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (isTechRequiredForUnit((eTech), (UnitTypes)iI))
		{
			int iTempValue = (GC.getUnitInfo((UnitTypes)iI).getCombat() * 100) / std::max(1, (GC.getGame().getBestLandUnitCombat()));
			iTempValue *= bAnyWarplan ? 2 : 1;

			iValue += iTempValue / 3;
			iBestUnitValue = std::max(iBestUnitValue, iTempValue);
		}
	}
	iValue += std::max(0, iBestUnitValue - 15);

	//cultural things
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		const BuildingTypes eLoopBuilding = static_cast<BuildingTypes>(iI);

		if (isTechRequiredForBuilding(eTech, eLoopBuilding))
		{
			const CvBuildingInfo& kLoopBuilding = GC.getBuildingInfo(eLoopBuilding);

			iValue += 15 * (kLoopBuilding.getCommerceChange(COMMERCE_CULTURE) + kLoopBuilding.getCommercePerPopChange(COMMERCE_CULTURE)) / 2;
			iValue += kLoopBuilding.getCommerceModifier(COMMERCE_CULTURE) * 2;
		}
	}

	//important civics
	for (int iI = 0; iI < GC.getNumCivicInfos(); iI++)
	{
		if (GC.getCivicInfo((CivicTypes)iI).getTechPrereq() == eTech)
		{
			iValue += GC.getCivicInfo((CivicTypes)iI).getCommerceModifier(COMMERCE_CULTURE) * 2;
		}
	}

	return iValue;
}

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  04/25/10								jdog5000	  */
/*																							  */
/* Victory Strategy AI																		  */
/************************************************************************************************/
int CvPlayerAI::AI_getCultureVictoryStage() const
{

	PROFILE_EXTRA_FUNC();
	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_CULTURE") <= 0 || !GC.getGame().culturalVictoryValid())
	{
		return 0;
	}

	// Necessary as capital city pointer is used later
	if (getCapitalCity() == NULL)
	{
		return 0;
	}

	int iHighCultureCount = 0;
	int iCloseToLegendaryCount = 0;
	int iLegendaryCount = 0;

	foreach_(const CvCity * cityX, cities())
	{
		if (cityX->getCultureLevel() >= GC.getGame().culturalVictoryCultureLevel() - 1)
		{
			if (cityX->getBaseCommerceRate(COMMERCE_CULTURE) > 100)
			{
				iHighCultureCount++;
			}
			const int iVictoryThreshold = GC.getGame().getCultureThreshold(GC.getGame().culturalVictoryCultureLevel());

			// is already there?
			if (cityX->getCulture(getID()) > iVictoryThreshold)
			{
				iLegendaryCount++;
			}
			// is over 1/2 of the way there?
			else if (cityX->getCulture(getID()) > iVictoryThreshold / 2)
			{
				iCloseToLegendaryCount++;
			}
		}
	}

	if (iLegendaryCount >= GC.getGame().culturalVictoryNumCultureCities())
	{
		// Already won, keep playing culture heavy but do some tech to keep pace if human wants to keep playing
		return 3;
	}
	if (iCloseToLegendaryCount >= GC.getGame().culturalVictoryNumCultureCities())
	{
		return 4;
	}

	if (isHumanPlayer())
	{
		if (getCommercePercent(COMMERCE_CULTURE) > 50)
		{
			return 3;
		}
		if (!GC.getGame().isDebugMode())
		{
			return 0;
		}
	}

	if (GC.getGame().getStartEra() > 1)
	{
		return 0;
	}

	if (getCapitalCity()->getGameTurnFounded() > (10 + GC.getGame().getStartTurn()))
	{
		if (iHighCultureCount < GC.getGame().culturalVictoryNumCultureCities())
		{
			//the loss of the capital indicates it might be a good idea to abort any culture victory
			return 0;
		}
	}

	int iValue = GC.getLeaderHeadInfo(getPersonalityType()).getCultureVictoryWeight();

	iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

	if (iValue > 20 && getNumCities() >= GC.getGame().culturalVictoryNumCultureCities())
	{
		iValue += 10 * countHolyCities();
	}

	iValue += (AI_getStrategyRand(0) % 100);

	if (iValue < 100)
	{
		return 0;
	}

	if (getCurrentEra() >= (GC.getNumEraInfos() - (2 + AI_getStrategyRand(1) % 2))) // K-mod
	{
		bool bAt3 = false;

		// if we have enough high culture cities, go to stage 3
		if (iHighCultureCount >= GC.getGame().culturalVictoryNumCultureCities())
		{
			bAt3 = true;
		}

		// if we have a lot of religion, may be able to catch up quickly
		if (countTotalHasReligion() >= getNumCities() * 3)
		{
			if (getNumCities() >= GC.getGame().culturalVictoryNumCultureCities())
			{
				bAt3 = true;
			}
		}

		if (bAt3)
		{
			if (AI_cultureVictoryTechValue(getCurrentResearch()) < 100)
			{
				return 4;
			}

			return 3;
		}
	}

	if (getCurrentEra() >= ((GC.getNumEraInfos() / 3) + AI_getStrategyRand(2) % 2)) // K-mod
	{
		return 2;
	}

	return 1;
}

int CvPlayerAI::AI_getSpaceVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue;

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_SPACE") <= 0)
	{
		return 0;
	}

	if (getCapitalCity() == NULL)
	{
		return 0;
	}

	// Better way to determine if spaceship victory is valid?
	VictoryTypes eSpace = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).getVictoryDelayTurns() > 0)
			{
				eSpace = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eSpace == NO_VICTORY)
	{
		return 0;
	}

	// If have built Apollo, then the race is on!
	bool bHasApollo = false;
	bool bNearAllTechs = true;
	for (int iI = 0; iI < GC.getNumProjectInfos(); iI++)
	{
		if (GC.getProjectInfo((ProjectTypes)iI).getVictoryPrereq() == eSpace)
		{
			if (GET_TEAM(getTeam()).getProjectCount((ProjectTypes)iI) > 0)
			{
				bHasApollo = true;
			}
			else
			{
				if (!GET_TEAM(getTeam()).isHasTech(GC.getProjectInfo((ProjectTypes)iI).getTechPrereq()))
				{
					if (!isResearchingTech(GC.getProjectInfo((ProjectTypes)iI).getTechPrereq()))
					{
						bNearAllTechs = false;
					}
				}
			}
		}
	}

	if (bHasApollo)
	{
		if (bNearAllTechs)
		{
			bool bOtherLaunched = false;
			if (GET_TEAM(getTeam()).getVictoryCountdown(eSpace) >= 0)
			{
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (iTeam != getTeam())
					{
						if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) >= 0)
						{
							if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) < GET_TEAM(getTeam()).getVictoryCountdown(eSpace))
							{
								bOtherLaunched = true;
								break;
							}

							if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) == GET_TEAM(getTeam()).getVictoryCountdown(eSpace) && (iTeam < getTeam()))
							{
								bOtherLaunched = true;
								break;
							}
						}
					}
				}
			}
			else
			{
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (GET_TEAM((TeamTypes)iTeam).getVictoryCountdown(eSpace) >= 0)
					{
						bOtherLaunched = true;
						break;
					}
				}
			}

			if (!bOtherLaunched)
			{
				return 4;
			}

			return 3;
		}

		if (GET_TEAM(getTeam()).getBestKnownTechScorePercent() > (m_iVictoryStrategyHash & AI_VICTORY_SPACE3 ? 80 : 85))
		{
			return 3;
		}
	}

	if (isHumanPlayer() && !(GC.getGame().isDebugMode()))
	{
		return 0;
	}

	// If can't build Apollo yet, then consider making player push for this victory type
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getSpaceVictoryWeight();

		if (GET_TEAM(getTeam()).isAVassal())
		{
			iValue += 20;
		}

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

		iValue += (AI_getStrategyRand(3) % 100);

		if (iValue >= 100)
		{
			if (getCurrentEra() >= GC.getNumEraInfos() - 3)
			{
				return 2;
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getConquestVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue;

	if (GET_TEAM(getTeam()).isAVassal())
	{
		return 0;
	}

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_CONQUEST") <= 0)
	{
		return 0;
	}

	VictoryTypes eConquest = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).isConquest())
			{
				eConquest = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eConquest == NO_VICTORY)
	{
		return 0;
	}

	// Check for whether we are very powerful, looking good for conquest
	int iOurPower = GET_TEAM(getTeam()).getPower(true);
	int iOurPowerRank = 1;
	int iTotalPower = 0;
	int iNumNonVassals = 0;
	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		if (iI != getTeam())
		{
			CvTeam& kTeam = GET_TEAM((TeamTypes)iI);
			if (kTeam.isAlive() && !(kTeam.isMinorCiv()))
			{
				if (!kTeam.isAVassal())
				{
					iTotalPower += kTeam.getPower(true);
					iNumNonVassals++;

					if (GET_TEAM(getTeam()).isHasMet((TeamTypes)iI))
					{
						if (95 * kTeam.getPower(false) > 100 * iOurPower)
						{
							iOurPowerRank++;
						}
					}
				}
			}
		}
	}
	int iAverageOtherPower = iTotalPower / std::max(1, iNumNonVassals);

	if (3 * iOurPower > 4 * iAverageOtherPower)
	{
		// BBAI TODO: Have we declared total war on anyone?  Need some aggressive action taken, maybe past war success
		int iOthersWarMemoryOfUs = 0;
		for (int iPlayer = 0; iPlayer < MAX_PC_PLAYERS; iPlayer++)
		{
			if (GET_PLAYER((PlayerTypes)iPlayer).getTeam() != getTeam() && GET_PLAYER((PlayerTypes)iPlayer).isEverAlive())
			{
				iOthersWarMemoryOfUs += GET_PLAYER((PlayerTypes)iPlayer).AI_getMemoryCount(getID(), MEMORY_DECLARED_WAR);
			}
		}

		if (GET_TEAM(getTeam()).getHasMetCivCount(false) > GC.getGame().countCivPlayersAlive() / 4)
		{
			if (iOurPowerRank <= 1 + (GET_TEAM(getTeam()).getHasMetCivCount(true) / 10))
			{
				if ((iOurPower > 2 * iAverageOtherPower) && (iOurPower - iAverageOtherPower > 100))
				{
					if (iOthersWarMemoryOfUs > 0)
					{
						return 4;
					}
				}
			}
		}

		if (getCurrentEra() >= ((GC.getNumEraInfos() / 3)))
		{
			if (iOurPowerRank <= 1 + (GET_TEAM(getTeam()).getHasMetCivCount(true) / 7))
			{
				if (iOthersWarMemoryOfUs > 2)
				{
					return 3;
				}
			}
		}
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	// Check for whether we are inclined to pursue a conquest strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getConquestVictoryWeight();

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 20 : 0);

		iValue += (AI_getStrategyRand(4) % 100); // K-mod

		if (iValue >= 100)
		{
			if (m_iStrategyHash & AI_STRATEGY_GET_BETTER_UNITS)
			{
				if ((getNumCities() > 3) && (4 * iOurPower > 5 * iAverageOtherPower))
				{
					return 2;
				}
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getDominationVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	if (GET_TEAM(getTeam()).isAVassal())
	{
		return 0;
	}

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_DOMINATION") <= 0)
	{
		return 0;
	}

	VictoryTypes eDomination = NO_VICTORY;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.getLandPercent() > 0 && kVictoryInfo.getPopulationPercentLead())
			{
				eDomination = (VictoryTypes)iI;
				break;
			}
		}
	}

	if (eDomination == NO_VICTORY)
	{
		return 0;
	}

	int iPercentOfDomination = 0;
	int iOurPopPercent = (100 * GET_TEAM(getTeam()).getTotalPopulation()) / std::max(1, GC.getGame().getTotalPopulation());
	int iOurLandPercent = (100 * GET_TEAM(getTeam()).getTotalLand()) / std::max(1, GC.getMap().getLandPlots());

	iPercentOfDomination = (100 * iOurPopPercent) / std::max(1, GC.getGame().getAdjustedPopulationPercent(eDomination));
	iPercentOfDomination = std::min(iPercentOfDomination, (100 * iOurLandPercent) / std::max(1, GC.getGame().getAdjustedLandPercent(eDomination)));


	if (iPercentOfDomination > 80)
	{
		return 4;
	}

	if (iPercentOfDomination > 50)
	{
		return 3;
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	// Check for whether we are inclined to pursue a domination strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getDominationVictoryWeight();

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 20 : 0);

		iValue += (AI_getStrategyRand(5) % 100); // K-mod

		if (iValue >= 100)
		{
			if (getNumCities() > 3 && (GC.getGame().getPlayerRank(getID()) < (GC.getGame().countCivPlayersAlive() + 1) / 2))
			{
				return 2;
			}

			return 1;
		}
	}

	return 0;
}

int CvPlayerAI::AI_getDiplomacyVictoryStage() const
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	if (GC.getDefineINT("BBAI_VICTORY_STRATEGY_DIPLOMACY") <= 0)
	{
		return 0;
	}

	std::vector<VictoryTypes> veDiplomacy;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			if (GC.getVictoryInfo((VictoryTypes)iI).isDiploVote())
			{
				veDiplomacy.push_back((VictoryTypes)iI);
			}
		}
	}

	if (veDiplomacy.empty())
	{
		return 0;
	}

	// Check for whether we are elligible for election
	bool bVoteEligible = false;
	for (int iVoteSource = 0; iVoteSource < GC.getNumVoteSourceInfos(); iVoteSource++)
	{
		if (GC.getGame().isDiploVote((VoteSourceTypes)iVoteSource))
		{
			if (GC.getGame().isTeamVoteEligible(getTeam(), (VoteSourceTypes)iVoteSource))
			{
				bVoteEligible = true;
				break;
			}
		}
	}

	bool bDiploInclined = false;

	// Check for whether we are inclined to pursue a diplomacy strategy
	{
		iValue = GC.getLeaderHeadInfo(getPersonalityType()).getDiplomacyVictoryWeight();

		iValue += (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? -20 : 0);

		iValue += (AI_getStrategyRand(6) % 100); // K-mod

		// BBAI TODO: Level 2?

		if (iValue >= 100)
		{
			bDiploInclined = true;
		}
	}

	if (bVoteEligible && (bDiploInclined || isHumanPlayer()))
	{
		// BBAI TODO: Level 4 - close to enough to win a vote?

		return 3;
	}

	if (isHumanPlayer() && !GC.getGame().isDebugMode())
	{
		return 0;
	}

	if (bDiploInclined)
	{
		return 1;
	}

	return 0;
}

/// \brief Returns whether player is pursuing a particular victory strategy.
///
/// Victory strategies are computed on demand once per turn and stored for the rest
/// of the turn.  Each victory strategy type has 4 levels, the first two are
/// determined largely from AI tendencies and randomn dice rolls.  The second
/// two are based on measurables and past actions, so the AI can use them to
/// determine what other players (including the human player) are doing.
bool CvPlayerAI::AI_isDoVictoryStrategy(int iVictoryStrategy) const
{
	if (isNPC() || isMinorCiv() || !isAlive())
	{
		return false;
	}

	return (iVictoryStrategy & AI_getVictoryStrategyHash());
}

bool CvPlayerAI::AI_isDoVictoryStrategyLevel4() const
{
	if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION4))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DIPLOMACY4))
	{
		return true;
	}

	return false;
}

bool CvPlayerAI::AI_isDoVictoryStrategyLevel3() const
{
	if (AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3))
	{
		return true;
	}

	if (AI_isDoVictoryStrategy(AI_VICTORY_DIPLOMACY3))
	{
		return true;
	}

	return false;
}

void CvPlayerAI::AI_forceUpdateVictoryStrategies()
{
	//this forces a recache.
	m_iVictoryStrategyHashCacheTurn = -1;
}

int CvPlayerAI::AI_getVictoryStrategyHash() const
{
	PROFILE_FUNC();

	if (isNPC() || isMinorCiv() || !isAlive())
	{
		return 0;
	}

	if ((m_iVictoryStrategyHash != 0) && (m_iVictoryStrategyHashCacheTurn == GC.getGame().getGameTurn()))
	{
		return m_iVictoryStrategyHash;
	}

	m_iVictoryStrategyHash = AI_DEFAULT_VICTORY_STRATEGY;
	m_iVictoryStrategyHashCacheTurn = GC.getGame().getGameTurn();

	if (getCapitalCity() == NULL)
	{
		return m_iVictoryStrategyHash;
	}

	bool bStartedOtherLevel3 = false;
	bool bStartedOtherLevel4 = false;

	// Space victory
	int iVictoryStage = AI_getSpaceVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_SPACE1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_SPACE2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_SPACE3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_SPACE4;
				}
			}
		}
	}

	// Conquest victory
	iVictoryStage = AI_getConquestVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_CONQUEST4;
				}
			}
		}
	}

	// Domination victory
	iVictoryStage = AI_getDominationVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_DOMINATION4;
				}
			}
		}
	}

	// Cultural victory
	iVictoryStage = AI_getCultureVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_CULTURE1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_CULTURE2;
			if (iVictoryStage > 2)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_CULTURE3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_CULTURE4;
				}
			}
		}
	}

	// Diplomacy victory
	iVictoryStage = AI_getDiplomacyVictoryStage();

	if (iVictoryStage >= 1)
	{
		m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY1;
		if (iVictoryStage > 1)
		{
			m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY2;
			if (iVictoryStage > 2 && !bStartedOtherLevel3)
			{
				bStartedOtherLevel3 = true;
				m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY3;

				if (iVictoryStage > 3 && !bStartedOtherLevel4)
				{
					bStartedOtherLevel4 = true;
					m_iVictoryStrategyHash |= AI_VICTORY_DIPLOMACY4;
				}
			}
		}
	}

	return m_iVictoryStrategyHash;
}
/************************************************************************************************/
/* BETTER_BTS_AI_MOD					   END												  */
/************************************************************************************************/

/************************************************************************************************/
/* BETTER_BTS_AI_MOD					  03/18/10								jdog5000	  */
/*																							  */
/* War Strategy AI																			  */
/************************************************************************************************/
// AIAndy: Calculate strategy rand in separate method to control when it is computed for MP
void CvPlayerAI::AI_calculateStrategyRand()
{
	if (m_iStrategyRand <= 0)
	{
		m_iStrategyRand = 1 + GC.getGame().getSorenRandNum(100000, "AI Strategy Rand");
	}
}

// K-Mod, based on BBAI
int CvPlayerAI::AI_getStrategyRand(int iShift) const
{
	PROFILE_EXTRA_FUNC();
	const unsigned iBits = 16;

	iShift += getCurrentEra();
	while (iShift < 0)
		iShift += iBits;
	iShift %= iBits;

	if (m_iStrategyRand <= 0)
	{
		m_iStrategyRand = GC.getGame().getSorenRandNum((1 << (iBits + 1)) - 1, "AI Strategy Rand");
	}

	return (m_iStrategyRand << iShift) + (m_iStrategyRand >> (iBits - iShift));
}

bool CvPlayerAI::AI_isDoStrategy(int iStrategy) const
{
	if (isHumanPlayer() || isNPC() || isMinorCiv() || !isAlive())
	{
		return false;
	}
	return (iStrategy & AI_getStrategyHash());
}

void CvPlayerAI::AI_forceUpdateStrategies()
{
	//this forces a recache.
	m_iStrategyHashCacheTurn = -1;
}

int CvPlayerAI::AI_getStrategyHash() const
{
	PROFILE_FUNC();

	if ((m_iStrategyHash != 0) && (m_iStrategyHashCacheTurn == GC.getGame().getGameTurn()))
	{
		return m_iStrategyHash;
	}

	const FlavorTypes AI_FLAVOR_MILITARY = (FlavorTypes)0;
	const FlavorTypes AI_FLAVOR_RELIGION = (FlavorTypes)1;
	const FlavorTypes AI_FLAVOR_PRODUCTION = (FlavorTypes)2;
	const FlavorTypes AI_FLAVOR_GOLD = (FlavorTypes)3;
	const FlavorTypes AI_FLAVOR_SCIENCE = (FlavorTypes)4;
	const FlavorTypes AI_FLAVOR_CULTURE = (FlavorTypes)5;
	const FlavorTypes AI_FLAVOR_GROWTH = (FlavorTypes)6;

	int iLastStrategyHash = m_iStrategyHash;

	m_iStrategyHash = AI_DEFAULT_STRATEGY;
	m_iStrategyHashCacheTurn = GC.getGame().getGameTurn();

	if (AI_getFlavorValue(AI_FLAVOR_PRODUCTION) >= 2) // 0, 2, 5 or 10 in default xml [augustus 5, frederick 10, huayna 2, jc 2, chinese leader 2, qin 5, ramsess 2, roosevelt 5, stalin 2]
	{
		m_iStrategyHash |= AI_STRATEGY_PRODUCTION;
	}

	if (!getCapitalCity())
	{
		return m_iStrategyHash;
	}

	CvTeamAI& team = GET_TEAM(getTeam());
	int iMetCount = team.getHasMetCivCount(true);

	//Unit Analysis
	int iBestSlowUnitCombat = -1;
	int iBestFastUnitCombat = -1;

	bool bHasMobileArtillery = false;
	bool bHasMobileAntiair = false;
	bool bHasBomber = false;

	int iNukeCount = 0;

	int iAttackUnitCount = 0;

	// K-Mod
	int iAverageEnemyUnit = 0;
	int iTypicalAttack = getTypicalUnitValue(UNITAI_ATTACK);
	int iTypicalDefence = getTypicalUnitValue(UNITAI_CITY_DEFENSE);
	// K-Mod end

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (getCapitalCity() && getCapitalCity()->canTrain((UnitTypes)iI))
		{
			const CvUnitInfo& unit = GC.getUnitInfo((UnitTypes)iI);
			const int iMoves = unit.getMoves();

			if (unit.getUnitAIType(UNITAI_RESERVE)
			|| unit.getUnitAIType(UNITAI_ATTACK_CITY)
			|| unit.getUnitAIType(UNITAI_COUNTER)
			|| unit.getUnitAIType(UNITAI_PILLAGE))
			{
				iAttackUnitCount++;

				if (iMoves == 1)
				{
					iBestSlowUnitCombat = std::max(iBestSlowUnitCombat, unit.getCombat());
				}
				else if (iMoves > 1)
				{
					iBestFastUnitCombat = std::max(iBestFastUnitCombat, unit.getCombat());
				}
			}
			// Mobile anti-air and artillery flags only meant for land units
			if (unit.getDomainType() == DOMAIN_LAND && iMoves > 1)
			{
				if (unit.getInterceptionProbability() > 25)
				{
					bHasMobileAntiair = true;
				}
				if (unit.getBombardRate() > 10)
				{
					bHasMobileArtillery = true;
				}
			}
			if (unit.getAirRange() > 1 && !unit.isSuicide()
			&& unit.getBombRate() > 10 && unit.getAirCombat() > 0)
			{
				bHasBomber = true;
			}
			if (unit.getNukeRange() > 0)
			{
				iNukeCount++;
			}
		}
	}

	// K-Mod
	{
		int iTotalPower = 0;
		int iTotalWeightedValue = 0;
		for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kPlayer.getTeam() != getTeam())
			{
				if (kPlayer.isAlive() && team.isHasMet(kPlayer.getTeam()))
				{
					// Attack units are scaled down to roughly reflect their limitations.
					// (eg. Knights (10) vs Macemen (8). Cavalry (15) vs Rifles (14). Tank (28) vs Infantry (20) / Marine (24) )
					int iValue = std::max(100 * kPlayer.getTypicalUnitValue(UNITAI_ATTACK) / 110, kPlayer.getTypicalUnitValue(UNITAI_CITY_DEFENSE));
					iTotalWeightedValue += kPlayer.getPower() * iValue;
					iTotalPower += kPlayer.getPower();
				}
			}
		}
		if (iTotalPower == 0)
			iAverageEnemyUnit = 0;
		else
			iAverageEnemyUnit = iTotalWeightedValue / iTotalPower;

		// A bit of random variation...
		iAverageEnemyUnit *= (91 + AI_getStrategyRand(1) % 20);
		iAverageEnemyUnit /= 100;
	}

	//if (iAttackUnitCount <= 1)
	if (iAttackUnitCount <= 1 || 100 * iAverageEnemyUnit > 140 * iTypicalAttack && 100 * iAverageEnemyUnit > 140 * iTypicalDefence)
	{
		m_iStrategyHash |= AI_STRATEGY_GET_BETTER_UNITS;
	}
	// K-Mod end

	if (iBestFastUnitCombat > iBestSlowUnitCombat)
	{
		m_iStrategyHash |= AI_STRATEGY_FASTMOVERS;
		if (bHasMobileArtillery && bHasMobileAntiair)
		{
			m_iStrategyHash |= AI_STRATEGY_LAND_BLITZ;
		}
	}
	if (iNukeCount > 0)
	{
		if ((GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb() + +AI_getStrategyRand(7) % 15) >= (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE) ? 37 : 43))
		{
			m_iStrategyHash |= AI_STRATEGY_OWABWNW;
		}
	}
	if (bHasBomber)
	{
		if (!(m_iStrategyHash & AI_STRATEGY_LAND_BLITZ))
		{
			m_iStrategyHash |= AI_STRATEGY_AIR_BLITZ;
		}
		else
		{
			if ((AI_getStrategyRand(8) % 2) == 0)
			{
				m_iStrategyHash |= AI_STRATEGY_AIR_BLITZ;
				m_iStrategyHash &= ~AI_STRATEGY_LAND_BLITZ;
			}
		}
	}

	if (gPlayerLogLevel >= 2)
	{
		if ((m_iStrategyHash & AI_STRATEGY_LAND_BLITZ) && !(iLastStrategyHash & AI_STRATEGY_LAND_BLITZ))
		{
			logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_LAND_BLITZ on turn %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn());
		}

		if ((m_iStrategyHash & AI_STRATEGY_AIR_BLITZ) && !(iLastStrategyHash & AI_STRATEGY_AIR_BLITZ))
		{
			logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_AIR_BLITZ on turn %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn());
		}
	}

	//missionary
	{
		const ReligionTypes eStateReligion = getStateReligion();
		if (eStateReligion != NO_RELIGION && hasHolyCity(eStateReligion))
		{
			int iMissionary = (
					AI_getFlavorValue(AI_FLAVOR_GROWTH) * 2 // up to 10
				+	AI_getFlavorValue(AI_FLAVOR_CULTURE) * 4 // up to 40
				+	AI_getFlavorValue(AI_FLAVOR_RELIGION) * 6 // up to 60
			);
			{
				const CivicTypes eCivic = (CivicTypes)GC.getLeaderHeadInfo(getPersonalityType()).getFavoriteCivic();
				if (eCivic != NO_CIVIC && (GC.getCivicInfo(eCivic).isNoNonStateReligionSpread()))
				{
					iMissionary += 20;
				}
			}
			iMissionary += 5*(countHolyCities() - 1) + 7*std::min(iMetCount, 5);

			for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
			{
				if (!GET_PLAYER((PlayerTypes)iI).isAlive()
				|| iI == getID()
				|| !team.isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam())
				|| !team.isOpenBorders(GET_PLAYER((PlayerTypes)iI).getTeam()))
				{
					continue;
				}
				if (GET_PLAYER((PlayerTypes)iI).getStateReligion() == eStateReligion)
				{
					iMissionary += 10;
				}
				else if (!GET_PLAYER((PlayerTypes)iI).isNoNonStateReligionSpread())
				{
					iMissionary += (GET_PLAYER((PlayerTypes)iI).countHolyCities() == 0) ? 12 : 4;
				}
			}
			{
				bool bHasHolyBuilding = false;
				const CvCity* holyCity = GC.getGame().getHolyCity(getStateReligion());
				if (holyCity && holyCity->getOwner() == getID())
				{
					foreach_(const BuildingTypes eTypeX, holyCity->getHasBuildings())
					{
						if (GC.getBuildingInfo(eTypeX).getGlobalReligionCommerce() == eStateReligion
						&& !holyCity->isDisabledBuilding(eTypeX))
						{
							if (holyCity->isActiveBuilding(eTypeX))
							{
								bHasHolyBuilding = true;
								break;
							}
						}
					}
				}
				if (!bHasHolyBuilding)
				{
					iMissionary -= 10;
				}
				else
				{
					iMissionary += 10;
				}
			}
			iMissionary += 3*(AI_getStrategyRand(9) % 7);

			if (iMissionary > 100)
			{
				m_iStrategyHash |= AI_STRATEGY_MISSIONARY;
			}
		}
	}

	// Espionage
	int iTempValue = 0;
	if (getCommercePercent(COMMERCE_ESPIONAGE) == 0)
	{
		iTempValue += 4;
	}

	if (!team.hasWarPlan(true))
	{
		iTempValue += (team.getBestKnownTechScorePercent() < 85) ? 5 : 3;
	}

	iTempValue += (100 - AI_getEspionageWeight()) / 10;

	iTempValue += AI_getStrategyRand(10) % 12; // K-mod

	if (iTempValue > 10)
	{
		m_iStrategyHash |= AI_STRATEGY_BIG_ESPIONAGE;
	}

	// Turtle strategy
	if (team.isAtWar() && getNumCities() > 0)
	{
		int iMaxWarCounter = 0;
		for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
		{
			if (iTeam != getTeam() && GET_TEAM((TeamTypes)iTeam).isAlive() && !GET_TEAM((TeamTypes)iTeam).isMinorCiv())
			{
				iMaxWarCounter = std::max(iMaxWarCounter, team.AI_getAtWarCounter((TeamTypes)iTeam));
			}
		}

		// Are we losing badly or recently attacked?
		if (team.AI_getWarSuccessCapitulationRatio() < -50 || iMaxWarCounter < 10)
		{
			if (team.AI_getEnemyPowerPercent(true) > std::max(150, GC.getDefineINT("BBAI_TURTLE_ENEMY_POWER_RATIO")))
			{
				m_iStrategyHash |= AI_STRATEGY_TURTLE;
			}
		}
	}

	if (gPlayerLogLevel >= 2)
	{
		if ((m_iStrategyHash & AI_STRATEGY_TURTLE) && !(iLastStrategyHash & AI_STRATEGY_TURTLE))
		{
			logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_TURTLE on turn %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn());
		}

		if (!(m_iStrategyHash & AI_STRATEGY_TURTLE) && (iLastStrategyHash & AI_STRATEGY_TURTLE))
		{
			logBBAI("	Player %d (%S) stops strategy AI_STRATEGY_TURTLE on turn %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn());
		}
	}

	int iCurrentEra = getCurrentEra();
	int iParanoia = 0;
	int iCloseTargets = 0;
	int iOurDefensivePower = team.getDefensivePower();

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !GET_PLAYER((PlayerTypes)iI).isMinorCiv())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam() && team.isHasMet(GET_PLAYER((PlayerTypes)iI).getTeam()))
			{
				if (!GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isAVassal() && !team.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
				{
					if (team.AI_getWarPlan(GET_PLAYER((PlayerTypes)iI).getTeam()) != NO_WARPLAN)
					{
						iCloseTargets++;
					}
					else if (!team.isVassal(GET_PLAYER((PlayerTypes)iI).getTeam()))
					{
						// Are they a threat?
						int iTempParanoia = 0;

						const int iTheirPower = GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true);

						if (4 * iTheirPower > 3 * iOurDefensivePower)
						{
							if (!GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).isAtWar()
							|| GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).AI_getEnemyPowerPercent(false) < 140)
							{
								// Memory of them declaring on us and our friends
								int iWarMemory = AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR);
								iWarMemory += (AI_getMemoryCount((PlayerTypes)iI, MEMORY_DECLARED_WAR_ON_FRIEND) + 1) / 2;

								if (iWarMemory > 0)
								{
									//they are a snake
									iTempParanoia += 50 + 50 * iWarMemory;

									if (gPlayerLogLevel >= 2)
									{
										logBBAI("	Player %d (%S) wary of %S because of war memory %d", getID(), getCivilizationDescription(0), GET_PLAYER((PlayerTypes)iI).getCivilizationDescription(0), iWarMemory);
									}
								}
							}
						}

						// Do we think our relations are bad?
						int iCloseness = AI_playerCloseness((PlayerTypes)iI, DEFAULT_PLAYER_CLOSENESS);
						if (iCloseness > 0)
						{
							int iAttitudeWarProb = 100 - GC.getLeaderHeadInfo(getPersonalityType()).getNoWarAttitudeProb(AI_getAttitude((PlayerTypes)iI));
							if (iAttitudeWarProb > 10)
							{
								if (4 * iTheirPower > 3 * iOurDefensivePower)
								{
									iTempParanoia += iAttitudeWarProb / 2;
								}

								iCloseTargets++;
							}

							if (iTheirPower > 2 * iOurDefensivePower)
							{
								if (AI_getAttitude((PlayerTypes)iI) != ATTITUDE_FRIENDLY)
								{
									iTempParanoia += 25;
								}
							}
						}

						if (iTempParanoia > 0)
						{
							iTempParanoia *= iTheirPower;
							iTempParanoia /= std::max(1, iOurDefensivePower);
						}

						// Do they look like they're going for militaristic victory?
						if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST4))
						{
							iTempParanoia += 200;
						}
						else if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_CONQUEST3))
						{
							iTempParanoia += 100;
						}
						else if (GET_PLAYER((PlayerTypes)iI).AI_isDoVictoryStrategy(AI_VICTORY_DOMINATION3))
						{
							iTempParanoia += 50;
						}

						if (iTempParanoia > 0)
						{
							if (iCloseness == 0)
							{
								iTempParanoia /= 2;
							}

							iParanoia += iTempParanoia;
						}
					}
				}
			}
		}
	}

	if (m_iStrategyHash & AI_STRATEGY_GET_BETTER_UNITS)
	{
		iParanoia *= 3;
		iParanoia /= 2;
	}

	// Scale paranoia in later eras/larger games
	//iParanoia -= (100*(iCurrentEra + 1)) / std::max(1, GC.getNumEraInfos());

	// K-Mod. You call that scaling for "later eras/larger games"? It isn't scaling, and it doesn't use the map size.
	// Lets try something else. Rough and ad hoc, but hopefully a bit better.
	iParanoia *= (3 * GC.getNumEraInfos() - 2 * iCurrentEra);
	iParanoia /= 3 * (std::max(1, GC.getNumEraInfos()));
	// That starts as a factor of 1, and drop to 1/3.  And now for game size...
	iParanoia *= 14;
	iParanoia /= (7 + std::max(team.getHasMetCivCount(true), GC.getWorldInfo(GC.getMap().getWorldSize()).getDefaultPlayers()));

	// Alert strategy
	if (iParanoia >= 200)
	{
		m_iStrategyHash |= AI_STRATEGY_ALERT1;
		if (iParanoia >= 400)
		{
			m_iStrategyHash |= AI_STRATEGY_ALERT2;
		}
	}

	// Economic focus (K-Mod) - Note: this strategy is a gambit. The goal is catch up in tech by avoiding building units.
	if (!team.hasWarPlan(true)
	&& 100 * iAverageEnemyUnit >= 150 * iTypicalAttack
	&& 100 * iAverageEnemyUnit >= 180 * iTypicalDefence)
	{
		m_iStrategyHash |= AI_STRATEGY_ECONOMY_FOCUS;
	}

	if (gPlayerLogLevel >= 2)
	{
		if ((m_iStrategyHash & AI_STRATEGY_ALERT1) && !(iLastStrategyHash & AI_STRATEGY_ALERT1))
		{
			logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_ALERT1 on turn %d with iParanoia %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iParanoia);
		}

		if (!(m_iStrategyHash & AI_STRATEGY_ALERT1) && (iLastStrategyHash & AI_STRATEGY_ALERT1))
		{
			logBBAI("	Player %d (%S) stops strategy AI_STRATEGY_ALERT1 on turn %d with iParanoia %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iParanoia);
		}

		if ((m_iStrategyHash & AI_STRATEGY_ALERT2) && !(iLastStrategyHash & AI_STRATEGY_ALERT2))
		{
			logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_ALERT2 on turn %d with iParanoia %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iParanoia);
		}

		if (!(m_iStrategyHash & AI_STRATEGY_ALERT2) && (iLastStrategyHash & AI_STRATEGY_ALERT2))
		{
			logBBAI("	Player %d (%S) stops strategy AI_STRATEGY_ALERT2 on turn %d with iParanoia %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iParanoia);
		}
	}

	if (!(AI_isDoVictoryStrategy(AI_VICTORY_CULTURE2))
		&& !(m_iStrategyHash & AI_STRATEGY_MISSIONARY)
		&& (iCurrentEra <= (2 + (AI_getStrategyRand(11) % 2))) && (iCloseTargets > 0))
	{
		int iDagger = 0;
		iDagger += 12000 / std::max(100, (50 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarRand()));
		iDagger *= (AI_getStrategyRand(12) % 11);
		iDagger /= 10;
		iDagger += 5 * std::min(8, AI_getFlavorValue(AI_FLAVOR_MILITARY));

		if (!GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iDagger += range(100 - GC.getHandicapInfo(GC.getGame().getHandicapType()).getAITrainPercent(), 0, 15);
		}

		if ((getCapitalCity()->area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || (getCapitalCity()->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
		{
			iDagger += (iAttackUnitCount > 0) ? 40 : 20;
		}

		if (isRebel())
		{
			iDagger += 30;
		}

		if (iDagger >= AI_DAGGER_THRESHOLD)
		{
			m_iStrategyHash |= AI_STRATEGY_DAGGER;
		}
		else
		{
			if (iLastStrategyHash &= AI_STRATEGY_DAGGER)
			{
				if (iDagger >= (9 * AI_DAGGER_THRESHOLD) / 10)
				{
					m_iStrategyHash |= AI_STRATEGY_DAGGER;
				}
			}
		}

		if (gPlayerLogLevel >= 2)
		{
			if ((m_iStrategyHash & AI_STRATEGY_DAGGER) && !(iLastStrategyHash & AI_STRATEGY_DAGGER))
			{
				logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_DAGGER on turn %d with iDagger %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iDagger);
			}

			if (!(m_iStrategyHash & AI_STRATEGY_DAGGER) && (iLastStrategyHash & AI_STRATEGY_DAGGER))
			{
				logBBAI("	Player %d (%S) stops strategy AI_STRATEGY_DAGGER on turn %d with iDagger %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iDagger);
			}
		}
	}

	if (!(m_iStrategyHash & AI_STRATEGY_ALERT2) && !(m_iStrategyHash & AI_STRATEGY_TURTLE))
	{//Crush
		int iWarCount = 0;
		int iCrushValue = 0;

		iCrushValue += AI_getStrategyRand(13) % (4 + AI_getFlavorValue(AI_FLAVOR_MILITARY) / 2);

		if (m_iStrategyHash & AI_STRATEGY_DAGGER)
		{
			iCrushValue += 3;
		}
		if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
		{
			iCrushValue += 3;
		}

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if ((GET_TEAM((TeamTypes)iI).isAlive()) && (iI != getTeam()))
			{
				if (team.AI_getWarPlan((TeamTypes)iI) != NO_WARPLAN)
				{
					if (!GET_TEAM((TeamTypes)iI).isAVassal())
					{
						if (team.AI_teamCloseness((TeamTypes)iI) > 0)
						{
							iWarCount++;
						}

						// K-Mod. (if we attack with our defenders, would they be beat their defenders?)
						if (100 * iTypicalDefence >= 110 * GET_TEAM((TeamTypes)iI).getTypicalUnitValue(UNITAI_CITY_DEFENSE))
						{
							iCrushValue += 2;
						}
					}

					if (team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_PREPARING_TOTAL)
					{
						iCrushValue += 6;
					}
					else if ((team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_TOTAL) && (team.AI_getWarPlanStateCounter((TeamTypes)iI) < 20))
					{
						iCrushValue += 6;
					}

					if ((team.AI_getWarPlan((TeamTypes)iI) == WARPLAN_DOGPILE) && (team.AI_getWarPlanStateCounter((TeamTypes)iI) < 20))
					{
						for (int iJ = 0; iJ < MAX_PC_TEAMS; iJ++)
						{
							if ((iJ != iI) && iJ != getTeam() && GET_TEAM((TeamTypes)iJ).isAlive())
							{
								if ((atWar((TeamTypes)iI, (TeamTypes)iJ)) && !GET_TEAM((TeamTypes)iI).isAVassal())
								{
									iCrushValue += 4;
								}
							}
						}
					}
					if (GET_TEAM((TeamTypes)iI).isRebelAgainst(getTeam()))
					{
						iCrushValue += 4;
					}
				}
			}
		}
		if ((iWarCount <= 1) && (iCrushValue >= ((iLastStrategyHash & AI_STRATEGY_CRUSH) ? 9 : 10)))
		{
			m_iStrategyHash |= AI_STRATEGY_CRUSH;
		}

		if (gPlayerLogLevel >= 2)
		{
			if ((m_iStrategyHash & AI_STRATEGY_CRUSH) && !(iLastStrategyHash & AI_STRATEGY_CRUSH))
			{
				logBBAI("	Player %d (%S) starts strategy AI_STRATEGY_CRUSH on turn %d with iCrushValue %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iCrushValue);
			}

			if (!(m_iStrategyHash & AI_STRATEGY_CRUSH) && (iLastStrategyHash & AI_STRATEGY_CRUSH))
			{
				logBBAI("	Player %d (%S) stops strategy AI_STRATEGY_CRUSH on turn %d with iCrushValue %d", getID(), getCivilizationDescription(0), GC.getGame().getGameTurn(), iCrushValue);
			}
		}
	}

	{
		CvTeamAI& kTeam = team;
		int iOurVictoryCountdown = kTeam.AI_getLowestVictoryCountdown();

		int iTheirVictoryCountdown = MAX_INT;

		for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
		{
			if ((GET_TEAM((TeamTypes)iI).isAlive()) && (iI != getTeam()))
			{
				CvTeamAI& kOtherTeam = GET_TEAM((TeamTypes)iI);
				iTheirVictoryCountdown = std::min(iTheirVictoryCountdown, kOtherTeam.AI_getLowestVictoryCountdown());
			}
		}

		if (MAX_INT == iTheirVictoryCountdown)
		{
			iTheirVictoryCountdown = -1;
		}

		if ((iOurVictoryCountdown >= 0) && (iTheirVictoryCountdown < 0 || iOurVictoryCountdown <= iTheirVictoryCountdown))
		{
			m_iStrategyHash |= AI_STRATEGY_LAST_STAND;
		}
		else if ((iTheirVictoryCountdown >= 0))
		{
			if ((iTheirVictoryCountdown < iOurVictoryCountdown))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
			else if (GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
			else if (AI_isDoVictoryStrategyLevel4() || AI_isDoVictoryStrategy(AI_VICTORY_SPACE3))
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
		}

		if (iOurVictoryCountdown < 0)
		{
			if (isCurrentResearchRepeat())
			{
				int iStronger = 0;
				int iAlive = 1;
				for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
				{
					if (iTeam != getTeam())
					{
						CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
						if (kLoopTeam.isAlive())
						{
							iAlive++;
							if (kTeam.getPower(true) < kLoopTeam.getPower(true))
							{
								iStronger++;
							}
						}
					}
				}

				if ((iStronger <= 1) || (iStronger <= iAlive / 4))
				{
					m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
				}
			}
		}

	}

	if (isCurrentResearchRepeat())
	{
		int iTotalVictories = 0;
		int iAchieveVictories = 0;
		int iWarVictories = 0;


		int iThreshold = std::max(1, (GC.getGame().countCivTeamsAlive() + 1) / 4);

		CvTeamAI& kTeam = team;
		for (int iVictory = 0; iVictory < GC.getNumVictoryInfos(); iVictory++)
		{
			const CvVictoryInfo& kVictory = GC.getVictoryInfo((VictoryTypes)iVictory);
			if (GC.getGame().isVictoryValid((VictoryTypes)iVictory))
			{
				iTotalVictories++;
				if (kVictory.isDiploVote())
				{
					//
				}
				else if (kVictory.isEndScore())
				{
					int iHigherCount = 0;
					int IWeakerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (GC.getGame().getTeamScore(getTeam()) < ((GC.getGame().getTeamScore((TeamTypes)iTeam) * 90) / 100))
								{
									iHigherCount++;
									if (kTeam.getPower(true) > kLoopTeam.getPower(true))
									{
										IWeakerCount++;
									}
								}
							}
						}
					}

					if (iHigherCount > 0)
					{
						if (IWeakerCount == iHigherCount)
						{
							iWarVictories++;
						}
					}
				}
				else if (kVictory.getCityCulture() > 0)
				{
					if (m_iStrategyHash & AI_VICTORY_CULTURE1)
					{
						iAchieveVictories++;
					}
				}
				else if (kVictory.getMinLandPercent() > 0 || kVictory.getLandPercent() > 0)
				{
					int iLargerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (kTeam.getTotalLand(true) < kLoopTeam.getTotalLand(true))
								{
									iLargerCount++;
								}
							}
						}
					}
					if (iLargerCount <= iThreshold)
					{
						iWarVictories++;
					}
				}
				else if (kVictory.isConquest())
				{
					int iStrongerCount = 0;
					for (int iTeam = 0; iTeam < MAX_PC_TEAMS; iTeam++)
					{
						if (iTeam != getTeam())
						{
							CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iTeam);
							if (kLoopTeam.isAlive())
							{
								if (kTeam.getPower(true) < kLoopTeam.getPower(true))
								{
									iStrongerCount++;
								}
							}
						}
					}
					if (iStrongerCount <= iThreshold)
					{
						iWarVictories++;
					}
				}
				else
				{
					if (kTeam.getVictoryCountdown((VictoryTypes)iVictory) > 0)
					{
						iAchieveVictories++;
					}
				}
			}
		}

		if (iAchieveVictories == 0)
		{
			if (iWarVictories > 0)
			{
				m_iStrategyHash |= AI_STRATEGY_FINAL_WAR;
			}
		}
	}
	return m_iStrategyHash;
}


void CvPlayerAI::AI_nowHasTech(TechTypes eTech)
{
	// while its _possible_ to do checks, for financial trouble, and this tech adds financial buildings
	// if in war and this tech adds important war units
	// etc
	// it makes more sense to just redetermine what to produce
	// that is already done every time a civ meets a new civ, it makes sense to do it when a new tech is learned
	// if this is changed, then at a minimum, AI_isFinancialTrouble should be checked
	if (!isHumanPlayer())
	{
		int iGameTurn = GC.getGame().getGameTurn();

		// only reset at most every 10 turns
		if (iGameTurn > m_iTurnLastProductionDirty + 10)
		{
			// redeterimine the best things to build in each city
			AI_makeProductionDirty();

			m_iTurnLastProductionDirty = iGameTurn;
		}
	}
}


int CvPlayerAI::AI_countDeadlockedBonuses(const CvPlot* plot0) const
{
	PROFILE_EXTRA_FUNC();

	const int iMinRange = GC.getGame().getModderGameOption(MODDERGAMEOPTION_MIN_CITY_DISTANCE);
	const int iRange = iMinRange * 2;
	int iCount = 0;

	for (int iDX = -iRange; iDX <= iRange; iDX++)
	{
		for (int iDY = -iRange; iDY <= iRange; iDY++)
		{
			if (plotDistance(iDX, iDY, 0, 0) > CITY_PLOTS_RADIUS)
			{
				const CvPlot* plotX = plotXY(plot0->getX(), plot0->getY(), iDX, iDY);

				if (plotX
				&&  plotX->getBonusType(getTeam()) != NO_BONUS
				&& !plotX->isCityRadius()
				&& (plotX->area() == plot0->area() || plotX->isWater()))
				{
					bool bCanFound = false;
					bool bNeverFound = true;
					// Potentially blockable resource, look for a city site within a city radius
					for (int iI = 0; iI < NUM_CITY_PLOTS; iI++)
					{
						const CvPlot* plotY = plotCity(plotX->getX(), plotX->getY(), iI);
						if (plotY)
						{
							//canFound usually returns very quickly
							if (canFound(plotY->getX(), plotY->getY(), false))
							{
								bNeverFound = false;
								if (stepDistance(plot0->getX(), plot0->getY(), plotY->getX(), plotY->getY()) > iMinRange)
								{
									bCanFound = true;
									break;
								}
							}
						}
					}
					if (!bNeverFound && !bCanFound)
					{
						iCount++;
					}
				}
			}
		}
	}
	return iCount;
}

int CvPlayerAI::AI_getOurPlotStrength(const CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const
{
	PROFILE_FUNC();

	int iValue = 0;

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange) | filtered(CvPlot::fn::area() == pPlot->area()))
	{
		const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());

		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			if (pLoopUnit->getOwner() == getID()
			&& (bDefensiveBonuses && pLoopUnit->canDefend() || pLoopUnit->canAttack())
			&& !pLoopUnit->isInvisible(getTeam(), false)
			&& (pLoopUnit->atPlot(pPlot) || pLoopUnit->canEnterPlot(pPlot) || pLoopUnit->canEnterPlot(pPlot, MoveCheck::Attack)))
			{
				if (!bTestMoves)
				{
					iValue += pLoopUnit->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
				}
				else if (pLoopUnit->baseMoves() >= iDistance)
				{
					iValue += pLoopUnit->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
				}
			}
		}
	}
	return iValue;
}

int CvPlayerAI::AI_getEnemyPlotStrength(const CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const
{
	PROFILE_FUNC();

	int iValue = 0;

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange)
	| filtered(CvPlot::fn::area() == pPlot->area()))
	{
		const int iDistance = stepDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());

		foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
		{
			if (atWar(pLoopUnit->getTeam(), getTeam()))
			{
				if ((bDefensiveBonuses && pLoopUnit->canDefend()) || pLoopUnit->canAttack())
				{
					if (!(pLoopUnit->canCoexistWithTeamOnPlot(getTeam(), *pPlot)))
					{
						if (pPlot->isValidDomainForAction(*pLoopUnit))
						{
							if (!bTestMoves)
							{
								iValue += pLoopUnit->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
							}
							else
							{
								int iDangerRange = pLoopUnit->baseMoves();
								iDangerRange += ((pLoopPlot->isValidRoute(pLoopUnit)) ? 1 : 0);
								if (iDangerRange >= iDistance)
								{
									iValue += pLoopUnit->currEffectiveStr((bDefensiveBonuses ? pPlot : NULL), NULL);
								}
							}
						}
					}
				}
			}
		}
	}

	return iValue;
}

int CvPlayerAI::AI_goldToUpgradeAllUnits(int iExpThreshold) const
{
	PROFILE_FUNC();

	if (m_iUpgradeUnitsCacheTurn == GC.getGame().getGameTurn() && m_iUpgradeUnitsCachedExpThreshold == iExpThreshold)
	{
		return m_iUpgradeUnitsCachedGold;
	}

	int iTotalGold = 0;

	// cache the value for each unit type
	std::vector<int> aiUnitUpgradePrice(GC.getNumUnitInfos(), 0);	// initializes to zeros

	foreach_(const CvUnit * unitX, units())
	{
		// if experience is below threshold, skip this unit
		if (unitX == NULL || unitX->isDelayedDeath() || unitX->getExperience() < iExpThreshold)
		{
			continue;
		}
		const UnitTypes eUnitType = unitX->getUnitType();

		// check cached value for this unit type
		const int iCachedUnitGold = aiUnitUpgradePrice[eUnitType];
		if (iCachedUnitGold != 0)
		{
			// if positive, add it to the sum
			if (iCachedUnitGold > 0)
			{
				iTotalGold += iCachedUnitGold;
			}

			// either way, done with this unit
			continue;
		}

		int iUnitGold = 0;
		int iUnitUpgradePossibilities = 0;

		const UnitAITypes eUnitAIType = unitX->AI_getUnitAIType();
		if (unitX->plot() != NULL)
		{
			const CvArea* pUnitArea = unitX->area();
			const int iUnitValue = AI_unitValue(eUnitType, eUnitAIType, pUnitArea);

			foreach_(int iUnitX, GC.getUnitInfo(eUnitType).getUnitUpgradeChain())
			{
				const UnitTypes eUnitY = (UnitTypes)iUnitX;
				// is it better?
				if (!GC.getUnitInfo(eUnitY).getNotUnitAIType(eUnitAIType)
				&& unitX->canUpgrade(eUnitY)
				&& AI_unitValue(eUnitY, eUnitAIType, pUnitArea) > iUnitValue)
				{
					// can we actually make this upgrade?
					iUnitGold += unitX->upgradePrice(eUnitY);
					iUnitUpgradePossibilities++;
				}
			}
		}

		// if we found any, find average and add to total
		if (iUnitUpgradePossibilities > 0)
		{
			iUnitGold /= iUnitUpgradePossibilities;

			// add to cache
			aiUnitUpgradePrice[eUnitType] = iUnitGold;

			// add to sum
			iTotalGold += iUnitGold;
		}
		else
		{
			// add to cache, dont upgrade to this type
			aiUnitUpgradePrice[eUnitType] = -1;
		}
	}

	m_iUpgradeUnitsCacheTurn = GC.getGame().getGameTurn();
	m_iUpgradeUnitsCachedExpThreshold = iExpThreshold;
	m_iUpgradeUnitsCachedGold = iTotalGold;

	return iTotalGold;
}

int CvPlayerAI::AI_goldTradeValuePercent() const
{
	return AI_isFinancialTrouble() ? 300 : 200;

}

int CvPlayerAI::AI_averageYieldMultiplier(YieldTypes eYield) const
{
	FASSERT_BOUNDS(0, NUM_YIELD_TYPES, eYield);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	FAssert(m_aiAverageYieldMultiplier[eYield] > 0);
	return m_aiAverageYieldMultiplier[eYield];
}

int CvPlayerAI::AI_averageCommerceMultiplier(CommerceTypes eCommerce) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	return m_aiAverageCommerceMultiplier[eCommerce];
}

int CvPlayerAI::AI_averageGreatPeopleMultiplier() const
{
	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}
	return m_iAverageGreatPeopleMultiplier;
}

//"100 eCommerce is worth (return) raw YIELD_COMMERCE
int CvPlayerAI::AI_averageCommerceExchange(CommerceTypes eCommerce) const
{
	FASSERT_BOUNDS(0, NUM_COMMERCE_TYPES, eCommerce);

	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		AI_calculateAverages();
	}

	return m_aiAverageCommerceExchange[eCommerce];
}

void CvPlayerAI::AI_calculateAverages() const
{
	PROFILE_EXTRA_FUNC();
	if (m_iAveragesCacheTurn != GC.getGame().getGameTurn())
	{
		for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
		{
			m_aiAverageYieldMultiplier[iI] = 0;
		}
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			m_aiAverageCommerceMultiplier[iI] = 0;
		}
		m_iAverageGreatPeopleMultiplier = 0;

		int64_t sumBaseCommerce[NUM_COMMERCE_TYPES];
		int64_t sumFinalCommerce[NUM_COMMERCE_TYPES];
		{
			int iTotalPopulation = 0;

			foreach_(const CvCity * cityX, cities())
			{
				const int iPopulation = std::max(cityX->getPopulation(), NUM_CITY_PLOTS);
				iTotalPopulation += iPopulation;

				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] += iPopulation * cityX->AI_yieldMultiplier((YieldTypes)iI);
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] += iPopulation * cityX->getTotalCommerceRateModifier((CommerceTypes)iI);

					sumBaseCommerce[iI] += cityX->getBaseCommerceRateTimes100((CommerceTypes)iI);
					sumFinalCommerce[iI] += cityX->getCommerceRateTimes100((CommerceTypes)iI);
				}
				m_iAverageGreatPeopleMultiplier += iPopulation * cityX->getTotalGreatPeopleRateModifier();
			}

			if (iTotalPopulation > 0)
			{
				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] = std::max(1, m_aiAverageYieldMultiplier[iI] / iTotalPopulation);
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] = std::max(1, m_aiAverageCommerceMultiplier[iI] / iTotalPopulation);
				}
				m_iAverageGreatPeopleMultiplier = std::max(1, m_iAverageGreatPeopleMultiplier / iTotalPopulation);
			}
			else
			{
				for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
				{
					m_aiAverageYieldMultiplier[iI] = 100;
				}
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					m_aiAverageCommerceMultiplier[iI] = 100;
				}
				m_iAverageGreatPeopleMultiplier = 100;
			}
		}
		//Calculate Exchange Rate
		for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
		{
			m_aiAverageCommerceExchange[iI] = (
				static_cast<int>(
					100 * std::max<int64_t>(1, sumBaseCommerce[iI])
					/ std::max<int64_t>(1, sumFinalCommerce[iI])
					)
			);
		}
		// Timestamp
		m_iAveragesCacheTurn = GC.getGame().getGameTurn();
	}
}

// K-Mod edition
void CvPlayerAI::AI_convertUnitAITypesForCrush()
{
	PROFILE_EXTRA_FUNC();
	std::map<int, int> spare_units;
	std::multimap<int, CvUnit*> ordered_units;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		// Keep 1/2 of recommended floating defenders.
		if (!pLoopArea || pLoopArea->getAreaAIType(getTeam()) == AREAAI_ASSAULT
			|| pLoopArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
		{
			spare_units[pLoopArea->getID()] = 0;
		}
		else
		{
			spare_units[pLoopArea->getID()] = (2 * AI_getTotalFloatingDefenders(pLoopArea) - AI_getTotalFloatingDefendersNeeded(pLoopArea)) / 2;
		}
	}

	foreach_(CvUnit * pLoopUnit, units())
	{
		bool bValid = false;

		if (pLoopUnit->AI_getUnitAIType() == UNITAI_RESERVE || pLoopUnit->AI_getUnitAIType() == UNITAI_COLLATERAL
		|| pLoopUnit->AI_isCityAIType() && (pLoopUnit->noDefensiveBonus() || pLoopUnit->getExtraCityDefensePercent() <= 30))
		{
			bValid = true;
		}

		/*if ((pLoopUnit->area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
		|| (pLoopUnit->area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
		{
		bValid = false;
		}*/

		if (!pLoopUnit->canAttack() || (pLoopUnit->AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
		{
			bValid = false;
		}

		if (spare_units[pLoopUnit->area()->getID()] <= 0)
			bValid = false;

		if (bValid)
		{
			if (pLoopUnit->plot()->isCity())
			{
				if (pLoopUnit->plot()->getPlotCity()->getOwner() == getID())
				{
					if (pLoopUnit->plot()->getBestDefender(getID()) == pLoopUnit)
					{
						bValid = false;
					}
				}
			}
			// Super Forts begin *AI_defense* - don't convert units guarding a fort
			else if (pLoopUnit->plot()->isCity(true))
			{
				if (pLoopUnit->plot()->getNumDefenders(pLoopUnit->getOwner()) == 1)
				{
					bValid = false;
				}
			}
			// Super Forts end
		}

		if (bValid)
		{
			int iValue = AI_unitValue(pLoopUnit->getUnitType(), UNITAI_ATTACK_CITY, pLoopUnit->area());
			ordered_units.insert(std::make_pair(iValue, pLoopUnit));
		}
	}

	// convert the highest scoring units first.
	std::multimap<int, CvUnit*>::reverse_iterator rit;
	for (rit = ordered_units.rbegin(); rit != ordered_units.rend(); ++rit)
	{
		if (rit->first > 0 && spare_units[rit->second->area()->getID()] > 0)
		{
			rit->second->AI_setUnitAIType(UNITAI_ATTACK_CITY);
			spare_units[rit->second->area()->getID()]--;
		}
	}
}

int CvPlayerAI::AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance) const
{
	PROFILE_FUNC();
	FAssert(GET_PLAYER(eIndex).isAlive());
	FAssert(eIndex != getID());

	int iValue = 0;
	foreach_(CvCity * pLoopCity, cities())
	{
		iValue += pLoopCity->AI_playerCloseness(eIndex, iMaxDistance);
	}

	return iValue;
}


int CvPlayerAI::AI_getTotalAreaCityThreat(const CvArea* pArea, int* piLargestThreat) const
{
	PROFILE_FUNC();

	int iValue = 0;
	int iLargestThreat = 0;

	foreach_(CvCity * pLoopCity, cities() | filtered(CvCity::fn::area() == pArea))
	{
		const int iThreat = pLoopCity->AI_cityThreat();
		iValue += iThreat;

		if (iThreat > iLargestThreat)
		{
			iLargestThreat = iThreat;
		}
	}

	if (piLargestThreat != NULL)
	{
		*piLargestThreat = iLargestThreat;
	}

	return iValue;
}

int CvPlayerAI::AI_countNumAreaHostileUnits(const CvArea* pArea, bool bPlayer, bool bTeam, bool bNeutral, bool bHostile, const CvPlot* pPlot, int iMaxDistance) const
{
	PROFILE_FUNC();
	CvPlot* pLoopPlot;
	int iCount;
	int iI;

	iCount = 0;

	for (iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		pLoopPlot = GC.getMap().plotByIndex(iI);
		if ((pLoopPlot->area() == pArea) && pLoopPlot->isVisible(getTeam(), false) && stepDistance(pLoopPlot->getX(), pLoopPlot->getY(), pPlot->getX(), pPlot->getY()) <= iMaxDistance &&
			((bPlayer && pLoopPlot->getOwner() == getID()) || (bTeam && pLoopPlot->getTeam() == getTeam())
				|| (bNeutral && !pLoopPlot->isOwned()) || (bHostile && pLoopPlot->isOwned() && GET_TEAM(getTeam()).isAtWar(pLoopPlot->getTeam()))))
		{
			iCount += pLoopPlot->plotCount(PUF_isEnemy, getID(), 0, NULL, NO_PLAYER, NO_TEAM, PUF_isVisible, getID());
		}
	}
	return iCount;
}

//this doesn't include the minimal one or two garrison units in each city.
int CvPlayerAI::AI_getTotalFloatingDefendersNeeded(const CvArea* pArea) const
{
	PROFILE_FUNC();

	const int iAreaCities = pArea->getCitiesPerPlayer(getID());
	const int iCurrentEra = std::max(0, getCurrentEra() - GC.getGame().getStartEra() / 2);

	int iDefenders = 1 + iAreaCities * (iCurrentEra + (GC.getGame().getMaxCityElimination() > 0 ? 3 : 2));
	iDefenders /= 3;
	iDefenders += pArea->getPopulationPerPlayer(getID()) / 7;

	if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE || pArea->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE || pArea->getAreaAIType(getTeam()) == AREAAI_MASSING)
	{
		if (!pArea->isHomeArea(getID()) && iAreaCities <= std::min(4, pArea->getNumCities() / 3))
		{
			// Land war here, as floating defenders are based on cities/population need to make sure
			// AI defends its footholds in new continents well.
			iDefenders += GET_TEAM(getTeam()).countEnemyPopulationByArea(pArea) / 14;
		}
	}

	if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
	{
		iDefenders *= 2;
	}
	else if (AI_isDoStrategy(AI_STRATEGY_ALERT2))
	{
		iDefenders *= 2;
	}
	else if (AI_isDoStrategy(AI_STRATEGY_ALERT1))
	{
		iDefenders *= 3;
		iDefenders /= 2;
	}
	else if (pArea->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
	{
		iDefenders *= 2;
		iDefenders /= 3;
	}
	else if (pArea->getAreaAIType(getTeam()) == AREAAI_MASSING)
	{
		if (GET_TEAM(getTeam()).AI_getEnemyPowerPercent(true) < (10 + GC.getLeaderHeadInfo(getPersonalityType()).getMaxWarNearbyPowerRatio()))
		{
			iDefenders *= 2;
			iDefenders /= 3;
		}
	}

	if (AI_getTotalAreaCityThreat(pArea) == 0)
	{
		iDefenders /= 2;
	}

	if (!GC.getGame().isOption(GAMEOPTION_AI_AGGRESSIVE))
	{
		iDefenders *= 2;
		iDefenders /= 3;
	}

	// Afforess - check finances
	if (!GET_TEAM(getTeam()).hasWarPlan(true) && AI_isFinancialTrouble())
	{
		iDefenders = std::max(1, iDefenders / 2);
	}

	// Removed AI_STRATEGY_GET_BETTER_UNITS reduction, it was reducing defenses twice

	if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE3))
	{
		iDefenders += 2 * iAreaCities;
		if (pArea->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE)
		{
			iDefenders *= 2; //go crazy
		}
	}

	iDefenders *= 60;
	iDefenders /= std::max(30, (GC.getHandicapInfo(GC.getGame().getHandicapType()).getAITrainPercent() - 20));

	if ((iCurrentEra < 3) && (GC.getGame().isOption(GAMEOPTION_BARBARIAN_RAGING)))
	{
		iDefenders += 2;
	}

	if (getCapitalCity() != NULL
	&&  getCapitalCity()->area() != pArea
	// Lessen defensive requirements only if not being attacked locally
	&& pArea->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE)
	{
		// This may be our first city captured on a large enemy continent,
		//	need defenses to scale up based on total number of area cities not just ours.
		iDefenders = std::min(iDefenders, iAreaCities * iAreaCities + pArea->getNumCities() - iAreaCities - 1);
	}

	// Build a few extra floating defenders for occupying forts
	iDefenders += iAreaCities / 2;

	return iDefenders;
}

int CvPlayerAI::AI_getTotalFloatingDefenders(const CvArea* pArea) const
{
	PROFILE_FUNC();
	int iCount = 0;

	iCount += AI_totalAreaUnitAIs(pArea, UNITAI_COLLATERAL);
	iCount += AI_totalAreaUnitAIs(pArea, UNITAI_RESERVE);
	iCount += std::max(0, (AI_totalAreaUnitAIs(pArea, UNITAI_CITY_DEFENSE) - (pArea->getCitiesPerPlayer(getID()) * 2)));
	iCount += AI_totalAreaUnitAIs(pArea, UNITAI_CITY_COUNTER);
	iCount += AI_totalAreaUnitAIs(pArea, UNITAI_CITY_SPECIAL);
	// BBAI TODO: Defense air?  Is this outdated?
	iCount += AI_totalAreaUnitAIs(pArea, UNITAI_DEFENSE_AIR);
	return iCount;
}

RouteTypes CvPlayerAI::AI_bestAdvancedStartRoute(const CvPlot* pPlot, int* piYieldValue) const
{
	PROFILE_EXTRA_FUNC();
	RouteTypes eBestRoute = NO_ROUTE;
	int iBestValue = -1;
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++)
	{
		RouteTypes eRoute = (RouteTypes)iI;

		int iValue = 0;
		int iCost = getAdvancedStartRouteCost(eRoute, true, pPlot);

		if (iCost >= 0)
		{
			iValue += GC.getRouteInfo(eRoute).getValue();

			if (iValue > 0)
			{
				int iYieldValue = 0;
				if (pPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					iYieldValue += ((GC.getImprovementInfo(pPlot->getImprovementType()).getRouteYieldChanges(eRoute, YIELD_FOOD)) * 100);
					iYieldValue += ((GC.getImprovementInfo(pPlot->getImprovementType()).getRouteYieldChanges(eRoute, YIELD_PRODUCTION)) * 60);
					iYieldValue += ((GC.getImprovementInfo(pPlot->getImprovementType()).getRouteYieldChanges(eRoute, YIELD_COMMERCE)) * 40);
				}
				iValue *= 1000;
				iValue /= (1 + iCost);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestRoute = eRoute;
					if (piYieldValue != NULL)
					{
						*piYieldValue = iYieldValue;
					}
				}
			}
		}
	}
	return eBestRoute;
}

UnitTypes CvPlayerAI::AI_bestAdvancedStartUnitAI(const CvPlot* pPlot, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(eUnitAI != NO_UNITAI, "UnitAI is not assigned a valid value");

	int iValue;
	int iBestValue = 0;
	UnitTypes eBestUnit = NO_UNIT;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

		int iUnitCost = getAdvancedStartUnitCost((UnitTypes)iI, true, pPlot);
		if (iUnitCost >= 0)
		{
			iValue = AI_unitValue((UnitTypes)iI, eUnitAI, pPlot->area());

			if (iValue > 0)
			{
				//free promotions. slow?
				//only 1 promotion per source is counted (ie protective isn't counted twice)
				int iPromotionValue = 0;

				const UnitCombatTypes eUnitCombat = (UnitCombatTypes)kUnit.getUnitCombatType();

				for (int iJ = 0; iJ < GC.getNumPromotionInfos(); iJ++)
				{
					if (kUnit.getFreePromotions(iJ))
					{
						iPromotionValue += 15;
					}
					else if (isFreePromotion((UnitTypes)iI, (PromotionTypes)iJ))
					{
						iPromotionValue += 15;
					}
					else if (eUnitCombat != NO_UNITCOMBAT)
					{
						if (isFreePromotion(eUnitCombat, (PromotionTypes)iJ))
						{
							iPromotionValue += 15;
							continue;
						}
						for (int iK = 0; iK < GC.getNumTraitInfos(); iK++)
						{
							if (hasTrait((TraitTypes)iK)
							&& GC.getTraitInfo((TraitTypes)iK).isFreePromotionUnitCombats(iJ, (int)eUnitCombat))
							{
								iPromotionValue += 15;
								break;
							}
						}
					}
				}
				iValue *= (iPromotionValue + 100);
				iValue /= 100;

				iValue *= (GC.getGame().getSorenRandNum(40, "AI Best Advanced Start Unit") + 100);
				iValue /= 100;

				iValue *= (getNumCities() + 2);
				iValue /= (getUnitCountPlusMaking((UnitTypes)iI) + getNumCities() + 2);

				FAssert((MAX_INT / 1000) > iValue);
				iValue *= 1000;

				iValue /= 1 + iUnitCost;

				iValue = std::max(1, iValue);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestUnit = (UnitTypes)iI;
				}
			}
		}
	}
	return eBestUnit;
}

CvPlot* CvPlayerAI::AI_advancedStartFindCapitalPlot() const
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pBestPlot = NULL;
	int iBestValue = -1;

	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kPlayer.isAlive())
		{
			if (kPlayer.getTeam() == getTeam())
			{
				CvPlot* pLoopPlot = kPlayer.getStartingPlot();
				if (pLoopPlot != NULL)
				{
					if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
					{
						int iX = pLoopPlot->getX();
						int iY = pLoopPlot->getY();

						int iValue = 1000;
						if (iPlayer == getID())
						{
							iValue += 1000;
						}
						else
						{
							iValue += GC.getGame().getSorenRandNum(100, "AI Advanced Start Choose Team Start");
						}
						CvCity* pNearestCity = GC.getMap().findCity(iX, iY, NO_PLAYER, getTeam());
						if (NULL != pNearestCity)
						{
							FAssert(pNearestCity->getTeam() == getTeam());
							int iDistance = stepDistance(iX, iY, pNearestCity->getX(), pNearestCity->getY());
							if (iDistance < 10)
							{
								iValue /= (10 - iDistance);
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
				else
				{
					FErrorMsg("StartingPlot for a live player is NULL!");
				}
			}
		}
	}

	if (pBestPlot != NULL)
	{
		return pBestPlot;
	}

	FErrorMsg("AS: Failed to find a starting plot for a player");

	//Execution should almost never reach here.

	//Update found values just in case - particulary important for simultaneous turns.
	AI_updateFoundValues(true);

	pBestPlot = NULL;
	iBestValue = -1;

	if (NULL != getStartingPlot())
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			if (pLoopPlot->getArea() == getStartingPlot()->getArea())
			{
				int iValue = pLoopPlot->getFoundValue(getID());
				if (iValue > 0)
				{
					if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
					{
						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
						}
					}
				}
			}
		}
	}

	if (pBestPlot != NULL)
	{
		return pBestPlot;
	}

	//Commence panic.
	FErrorMsg("Failed to find an advanced start starting plot");
	return NULL;
}


bool CvPlayerAI::AI_advancedStartPlaceExploreUnits(bool bLand)
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pBestExplorePlot = NULL;
	int iBestExploreValue = 0;
	UnitTypes eBestUnitType = NO_UNIT;

	UnitAITypes eUnitAI = NO_UNITAI;
	if (bLand)
	{
		eUnitAI = UNITAI_EXPLORE;
	}
	else
	{
		eUnitAI = UNITAI_EXPLORE_SEA;
	}

	foreach_(const CvCity * pLoopCity, cities())
	{
		CvPlot* pLoopPlot = pLoopCity->plot();
		const CvArea* pLoopArea = bLand ? pLoopCity->area() : pLoopPlot->waterArea();

		if (pLoopArea != NULL)
		{
			int iValue = std::max(0, pLoopArea->getNumUnrevealedTiles(getTeam()) - 10) * 10;
			iValue += std::max(0, pLoopArea->getNumTiles() - 50);

			if (iValue > 0)
			{
				int iOtherPlotCount = 0;
				int iGoodyCount = 0;
				int iExplorerCount = 0;
				const int iAreaId = pLoopArea->getID();

				foreach_(const CvPlot * pLoopPlot2, pLoopPlot->rect(4, 4))
				{
					iExplorerCount += pLoopPlot2->plotCount(PUF_isUnitAIType, eUnitAI, -1, NULL, NO_PLAYER, getTeam());
					if (pLoopPlot2->getArea() == iAreaId)
					{
						if (pLoopPlot2->isGoody())
						{
							iGoodyCount++;
						}
						if (pLoopPlot2->getTeam() != getTeam())
						{
							iOtherPlotCount++;
						}
					}
				}

				iValue -= 300 * iExplorerCount;
				iValue += 200 * iGoodyCount;
				iValue += 10 * iOtherPlotCount;
				if (iValue > iBestExploreValue)
				{
					UnitTypes eUnit = AI_bestAdvancedStartUnitAI(pLoopPlot, eUnitAI);
					if (eUnit != NO_UNIT)
					{
						eBestUnitType = eUnit;
						iBestExploreValue = iValue;
						pBestExplorePlot = pLoopPlot;
					}
				}
			}
		}
	}

	if (pBestExplorePlot != NULL)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_UNIT, pBestExplorePlot->getX(), pBestExplorePlot->getY(), eBestUnitType, true);
		return true;
	}
	return false;
}

void CvPlayerAI::AI_advancedStartRevealRadius(const CvPlot* pPlot, int iRadius)
{
	PROFILE_EXTRA_FUNC();
	for (int iRange = 1; iRange <= iRadius; iRange++)
	{
		for (int iX = -iRange; iX <= iRange; iX++)
		{
			for (int iY = -iRange; iY <= iRange; iY++)
			{
				if (plotDistance(0, 0, iX, iY) <= iRadius)
				{
					CvPlot* pLoopPlot = plotXY(pPlot->getX(), pPlot->getY(), iX, iY);

					if (NULL != pLoopPlot)
					{
						if (getAdvancedStartVisibilityCost(pLoopPlot) > 0)
						{
							doAdvancedStartAction(ADVANCEDSTARTACTION_VISIBILITY, pLoopPlot->getX(), pLoopPlot->getY(), -1, true);
						}
					}
				}
			}
		}
	}
}

bool CvPlayerAI::AI_advancedStartPlaceCity(const CvPlot* pPlot)
{
	PROFILE_EXTRA_FUNC();
	//If there is already a city, then improve it.
	CvCity* pCity = pPlot->getPlotCity();
	if (pCity == NULL)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_CITY, pPlot->getX(), pPlot->getY(), -1, true);

		pCity = pPlot->getPlotCity();
		if (pCity == NULL || pCity->getOwner() != getID())
		{
			//this should never happen since the cost for a city should be 0 if
			//the city can't be placed.
			//(It can happen if another player has placed a city in the fog)
			FErrorMsg("ADVANCEDSTARTACTION_CITY failed in unexpected way");
			return false;
		}
	}

	//Only expand culture when we have lots to spare. Never expand for the capital, the palace works fine on it's own
	if (pCity != getCapitalCity()
	&& getAdvancedStartPoints() > getAdvancedStartCultureCost(true, pCity) * 50
	&& pCity->getCultureLevel() <= 1)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_CULTURE, pPlot->getX(), pPlot->getY(), -1, true);
		//to account for culture expansion.
		pCity->AI_updateBestBuild();
	}

	int iPlotsImproved = algo::count_if(pCity->plots(true),
		bind(CvPlot::getWorkingCity, _1) == pCity &&
		bind(CvPlot::getImprovementType, _1) != NO_IMPROVEMENT
	);

	int iDivisor = std::max(1, 2000 / std::max(1, getAdvancedStartPoints()));

	int iTargetPopulation = pCity->happyLevel() + (getCurrentEra() / 2);
	iTargetPopulation /= iDivisor;

	while (iPlotsImproved < iTargetPopulation)
	{
		const CvPlot* pBestPlot;
		ImprovementTypes eBestImprovement = NO_IMPROVEMENT;
		int iBestValue = 0;

		for (int iI = 0; iI < pCity->getNumCityPlots(); iI++)
		{
			const int iValue = pCity->AI_getBestBuildValue(iI);
			if (iValue > iBestValue)
			{
				const BuildTypes eBuild = pCity->AI_getBestBuild(iI);
				if (eBuild != NO_BUILD)
				{
					const ImprovementTypes eImprovement = GC.getBuildInfo(eBuild).getImprovement();
					if (eImprovement != NO_IMPROVEMENT)
					{
						const CvPlot* pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iI);
						if (pLoopPlot != NULL && pLoopPlot->getImprovementType() != eImprovement)
						{
							eBestImprovement = eImprovement;
							pBestPlot = pLoopPlot;
							iBestValue = iValue;
						}
					}
				}
			}
		}

		if (iBestValue < 1)
		{
			break;
		}
		FAssert(pBestPlot != NULL);
		doAdvancedStartAction(ADVANCEDSTARTACTION_IMPROVEMENT, pBestPlot->getX(), pBestPlot->getY(), eBestImprovement, true);
		iPlotsImproved++;
		if (pCity->getPopulation() < iPlotsImproved)
		{
			doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pBestPlot->getX(), pBestPlot->getY(), -1, true);
		}
	}

	while (iPlotsImproved > pCity->getPopulation())
	{
		const int iPopCost = getAdvancedStartPopCost(true, pCity);
		if (iPopCost <= 0 || iPopCost > getAdvancedStartPoints())
		{
			break;
		}
		if (pCity->healthRate() < 0)
		{
			break;
		}
		doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pPlot->getX(), pPlot->getY(), -1, true);
	}
	pCity->AI_updateAssignWork();

	return true;
}




//Returns false if we have no more points.
bool CvPlayerAI::AI_advancedStartDoRoute(const CvPlot* pFromPlot, const CvPlot* pToPlot)
{
	PROFILE_EXTRA_FUNC();
	FAssert(pFromPlot != NULL);
	FAssert(pToPlot != NULL);

	gDLL->getFAStarIFace()->ForceReset(&GC.getStepFinder());
	if (gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), pFromPlot->getX(), pFromPlot->getY(), pToPlot->getX(), pToPlot->getY(), false, -1, true))
	{
		FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder());
		if (pNode != NULL)
		{
			if (pNode->m_iData1 > (1 + stepDistance(pFromPlot->getX(), pFromPlot->getY(), pToPlot->getX(), pToPlot->getY())))
			{
				//Don't build convulted paths.
				return true;
			}
		}

		while (pNode != NULL)
		{
			CvPlot* pPlot = GC.getMap().plotSorenINLINE(pNode->m_iX, pNode->m_iY);
			RouteTypes eRoute = AI_bestAdvancedStartRoute(pPlot);
			if (eRoute != NO_ROUTE)
			{
				if (getAdvancedStartRouteCost(eRoute, true, pPlot) > getAdvancedStartPoints())
				{
					return false;
				}
				doAdvancedStartAction(ADVANCEDSTARTACTION_ROUTE, pNode->m_iX, pNode->m_iY, eRoute, true);
			}
			pNode = pNode->m_pParent;
		}
	}
	return true;
}
void CvPlayerAI::AI_advancedStartRouteTerritory()
{
	PROFILE_EXTRA_FUNC();
	CvPlot* pLoopPlot;

	for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		pLoopPlot = GC.getMap().plotByIndex(iI);
		if ((pLoopPlot != NULL) && (pLoopPlot->getOwner() == getID()) && (pLoopPlot->getRouteType() == NO_ROUTE))
		{
			if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				BonusTypes eBonus = pLoopPlot->getBonusType(getTeam());
				if (eBonus != NO_BONUS)
				{
					if (GC.getImprovementInfo(pLoopPlot->getImprovementType()).isImprovementBonusTrade(eBonus))
					{
						const int iBonusValue = AI_bonusVal(eBonus, 1);
						if (iBonusValue > 9)
						{
							int iBestValue = 0;
							const CvPlot* pBestPlot = NULL;
							foreach_(const CvPlot * pLoopPlot2, pLoopPlot->rect(2, 2))
							{
								if (pLoopPlot2->getOwner() == getID())
								{
									if (pLoopPlot2->isConnectedToCapital() || pLoopPlot2->isCity())
									{
										int iValue = 1000;
										if (pLoopPlot2->isCity())
										{
											iValue += 100;
											if (pLoopPlot2->getPlotCity()->isCapital())
											{
												iValue += 100;
											}
										}
										if (pLoopPlot2->isRoute())
										{
											iValue += 100;
										}
										const int iDistance = GC.getMap().calculatePathDistance(pLoopPlot, pLoopPlot2);
										if (iDistance > 0)
										{
											iValue /= (1 + iDistance);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = pLoopPlot2;
											}
										}
									}
								}
							}
							if (pBestPlot != NULL)
							{
								if (!AI_advancedStartDoRoute(pLoopPlot, pBestPlot))
								{
									return;
								}
							}
						}
					}
				}
				if (pLoopPlot->getRouteType() == NO_ROUTE)
				{
					int iRouteYieldValue = 0;
					const RouteTypes eRoute = AI_bestAdvancedStartRoute(pLoopPlot, &iRouteYieldValue);
					if (eRoute != NO_ROUTE && iRouteYieldValue > 0)
					{
						doAdvancedStartAction(ADVANCEDSTARTACTION_ROUTE, pLoopPlot->getX(), pLoopPlot->getY(), eRoute, true);
					}
				}
			}
		}
	}

	//Connect Cities
	foreach_(const CvCity * pLoopCity, cities()
	| filtered(!CvCity::fn::isCapital() && !CvCity::fn::isConnectedToCapital()))
	{
		int iBestValue = 0;
		CvPlot* pBestPlot = NULL;
		const int iRange = 5;
		foreach_(CvPlot * pLoopPlot, pLoopCity->plot()->rect(iRange, iRange)
		| filtered(CvPlot::fn::getOwner() == getID() && (CvPlot::fn::isConnectedToCapital(NO_PLAYER) || CvPlot::fn::isCity())))
		{
			int iValue = 1000;
			if (pLoopPlot->isCity())
			{
				iValue += 500;
				if (pLoopPlot->getPlotCity()->isCapital())
				{
					iValue += 500;
				}
			}
			if (pLoopPlot->isRoute())
			{
				iValue += 100;
			}
			const int iDistance = GC.getMap().calculatePathDistance(pLoopCity->plot(), pLoopPlot);
			if (iDistance > 0)
			{
				iValue /= (1 + iDistance);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					pBestPlot = pLoopPlot;
				}
			}
		}
		if (NULL != pBestPlot && !AI_advancedStartDoRoute(pBestPlot, pLoopCity->plot()))
		{
			return;
		}
	}
}


void CvPlayerAI::AI_doAdvancedStart(bool bNoExit)
{
	PROFILE_EXTRA_FUNC();
	FAssertMsg(!isNPC(), "Should not be called for NPCs!");

	if (NULL == getStartingPlot())
	{
		FErrorMsg("error");
		return;
	}

	int iStartingPoints = getAdvancedStartPoints();
	int iRevealPoints = (iStartingPoints * 10) / 100;
	int iMilitaryPoints = (iStartingPoints * (isHumanPlayer() ? 17 : (10 + (GC.getLeaderHeadInfo(getPersonalityType()).getBuildUnitProb() / 3)))) / 100;
	int iCityPoints = iStartingPoints - (iMilitaryPoints + iRevealPoints);

	if (getCapitalCity() != NULL)
	{
		AI_advancedStartPlaceCity(getCapitalCity()->plot());
	}
	else
	{
		for (int iPass = 0; iPass < 2 && NULL == getCapitalCity(); ++iPass)
		{
			CvPlot* pBestCapitalPlot = AI_advancedStartFindCapitalPlot();

			if (pBestCapitalPlot != NULL)
			{
				if (!AI_advancedStartPlaceCity(pBestCapitalPlot))
				{
					FErrorMsg("AS AI: Unexpected failure placing capital");
				}
				break;
			}
			//If this point is reached, the advanced start system is broken.
			//Find a new starting plot for this player
			setStartingPlot(findStartingPlot(), true);
			//Redo Starting visibility
			const CvPlot* pStartingPlot = getStartingPlot();
			if (NULL != pStartingPlot)
			{
				for (int iPlotLoop = 0; iPlotLoop < GC.getMap().numPlots(); ++iPlotLoop)
				{
					CvPlot* pPlot = GC.getMap().plotByIndex(iPlotLoop);

					if (plotDistance(pPlot->getX(), pPlot->getY(), pStartingPlot->getX(), pStartingPlot->getY()) <= GC.getDefineINT("ADVANCED_START_SIGHT_RANGE"))
					{
						pPlot->setRevealed(getTeam(), true, false, NO_TEAM, false);
					}
				}
			}
		}

		if (getCapitalCity() == NULL)
		{
			if (!bNoExit)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_EXIT, -1, -1, -1, true);
			}
			return;
		}
	}

	iCityPoints -= (iStartingPoints - getAdvancedStartPoints());

	int iLastPointsTotal = getAdvancedStartPoints();

	for (int iPass = 0; iPass < 6; iPass++)
	{
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			if (pLoopPlot->isRevealed(getTeam(), false))
			{
				if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
				{
					AI_advancedStartRevealRadius(pLoopPlot, CITY_PLOTS_RADIUS);
				}
				else
				{
					foreach_(const CvPlot * pLoopPlot2, pLoopPlot->cardinalDirectionAdjacent())
					{
						if (getAdvancedStartVisibilityCost(pLoopPlot2) > 0)
						{
							// Mildly maphackery but any smart human can see the terrain type of a tile.
							int iFoodYield = GC.getTerrainInfo(pLoopPlot2->getTerrainType()).getYield(YIELD_FOOD);
							if (pLoopPlot2->getFeatureType() != NO_FEATURE)
							{
								iFoodYield += GC.getFeatureInfo(pLoopPlot2->getFeatureType()).getYieldChange(YIELD_FOOD);
							}
							if (((iFoodYield >= 2) && !pLoopPlot2->isFreshWater()) || pLoopPlot2->isHills() || pLoopPlot2->isRiver())
							{
								doAdvancedStartAction(ADVANCEDSTARTACTION_VISIBILITY, pLoopPlot2->getX(), pLoopPlot2->getY(), -1, true);
							}
						}
					}
				}
			}
			if ((iLastPointsTotal - getAdvancedStartPoints()) > iRevealPoints)
			{
				break;
			}
		}
	}

	iLastPointsTotal = getAdvancedStartPoints();
	iCityPoints = std::min(iCityPoints, iLastPointsTotal);

	//Spend econ points on a tech?
	int iTechRand = 90 + GC.getGame().getSorenRandNum(20, "AI AS Buy Tech 1");
	int iTotalTechSpending = 0;

	if (getCurrentEra() == 0)
	{
		TechTypes eTech = AI_bestTech(1);
		if ((eTech != NO_TECH) && !GC.getTechInfo(eTech).isRepeat())
		{
			int iTechCost = getAdvancedStartTechCost(eTech, true);
			if (iTechCost > 0)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
				iTechRand -= 50;
				iTotalTechSpending += iTechCost;
			}
		}
	}

	bool bDonePlacingCities = false;
	for (int iPass = 0; iPass < 100; ++iPass)
	{
		const int iRand = iTechRand + 10 * getNumCities();
		if ((iRand > 0) && (GC.getGame().getSorenRandNum(100, "AI AS Buy Tech 2") < iRand))
		{
			const TechTypes eTech = AI_bestTech(1);
			if ((eTech != NO_TECH) && !GC.getTechInfo(eTech).isRepeat())
			{
				int iTechCost = getAdvancedStartTechCost(eTech, true);
				if ((iTechCost > 0) && ((iTechCost + iTotalTechSpending) < (iCityPoints / 4)))
				{
					doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
					iTechRand -= 50;
					iTotalTechSpending += iTechCost;

					foreach_(const CvCity * pLoopCity, cities())
					{
						AI_advancedStartPlaceCity(pLoopCity->plot());
					}
				}
			}
		}
		int iBestFoundValue = 0;
		CvPlot* pBestFoundPlot = NULL;
		AI_updateFoundValues(true);
		for (int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
			//if (pLoopPlot->area() == getStartingPlot()->area())
			{
				if (plotDistance(getStartingPlot()->getX(), getStartingPlot()->getY(), pLoopPlot->getX(), pLoopPlot->getY()) < 9)
				{
					if (pLoopPlot->getFoundValue(getID()) > iBestFoundValue)
					{
						if (getAdvancedStartCityCost(true, pLoopPlot) > 0)
						{
							pBestFoundPlot = pLoopPlot;
							iBestFoundValue = pLoopPlot->getFoundValue(getID());
						}
					}
				}
			}
		}

		if (iBestFoundValue < ((getNumCities() == 0) ? 1 : (500 + 250 * getNumCities())))
		{
			bDonePlacingCities = true;
		}
		if (!bDonePlacingCities)
		{
			const int iCost = getAdvancedStartCityCost(true, pBestFoundPlot);
			if (iCost > getAdvancedStartPoints())
			{
				bDonePlacingCities = true;
			}// at 500pts, we have 200, we spend 100.
			else if (((iLastPointsTotal - getAdvancedStartPoints()) + iCost) > iCityPoints)
			{
				bDonePlacingCities = true;
			}
		}

		if (!bDonePlacingCities)
		{
			if (!AI_advancedStartPlaceCity(pBestFoundPlot))
			{
				FErrorMsg("AS AI: Failed to place city (non-capital)");
				bDonePlacingCities = true;
			}
		}

		if (bDonePlacingCities)
		{
			break;
		}
	}


	bool bDoneWithTechs = false;
	while (!bDoneWithTechs)
	{
		bDoneWithTechs = true;
		const TechTypes eTech = AI_bestTech(1);
		if (eTech != NO_TECH && !GC.getTechInfo(eTech).isRepeat())
		{
			const int iTechCost = getAdvancedStartTechCost(eTech, true);
			if (iTechCost > 0 && iTechCost + iLastPointsTotal - getAdvancedStartPoints() <= iCityPoints)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_TECH, -1, -1, eTech, true);
				bDoneWithTechs = false;
			}
		}
	}

	//Land
	AI_advancedStartPlaceExploreUnits(true);
	if (getCurrentEra() > 2)
	{
		//Sea
		AI_advancedStartPlaceExploreUnits(false);
		if (GC.getGame().circumnavigationAvailable()
		&& GC.getGame().getSorenRandNum(GC.getGame().countCivPlayersAlive(), "AI AS buy 2nd sea explorer") < 2)
		{
			AI_advancedStartPlaceExploreUnits(false);
		}
	}

	AI_advancedStartRouteTerritory();

	bool bDoneBuildings = (iLastPointsTotal - getAdvancedStartPoints()) > iCityPoints;
	for (int iPass = 0; iPass < 10 && !bDoneBuildings; ++iPass)
	{
		foreach_(CvCity * pLoopCity, cities())
		{
			const BuildingTypes eBuilding = pLoopCity->AI_bestAdvancedStartBuilding(iPass);
			if (eBuilding != NO_BUILDING)
			{
				bDoneBuildings = (iLastPointsTotal - (getAdvancedStartPoints() - getAdvancedStartBuildingCost(eBuilding, true, pLoopCity))) > iCityPoints;
				if (!bDoneBuildings)
				{
					doAdvancedStartAction(ADVANCEDSTARTACTION_BUILDING, pLoopCity->getX(), pLoopCity->getY(), eBuilding, true);
				}
			}
		}
	}

	//Units
	std::vector<UnitAITypes> aeUnitAITypes;
	aeUnitAITypes.push_back(UNITAI_CITY_DEFENSE);
	aeUnitAITypes.push_back(UNITAI_WORKER);
	aeUnitAITypes.push_back(UNITAI_RESERVE);
	aeUnitAITypes.push_back(UNITAI_COUNTER);

	for (int iPass = 0; iPass < 10; ++iPass)
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			if (iPass == 0 || pLoopCity->getArea() == getStartingPlot()->getArea())
			{
				const CvPlot* pUnitPlot = pLoopCity->plot();
				//Token defender
				const UnitTypes eBestUnit = AI_bestAdvancedStartUnitAI(pUnitPlot, aeUnitAITypes[iPass % aeUnitAITypes.size()]);
				if (eBestUnit != NO_UNIT)
				{
					if (getAdvancedStartUnitCost(eBestUnit, true, pUnitPlot) > getAdvancedStartPoints())
					{
						break;
					}
					doAdvancedStartAction(ADVANCEDSTARTACTION_UNIT, pUnitPlot->getX(), pUnitPlot->getY(), eBestUnit, true);
				}
			}
		}
	}

	if (isHumanPlayer())
	{
		// remove unhappy population
		foreach_(const CvCity * pLoopCity, cities())
		{
			while (pLoopCity->angryPopulation() > 0 && getAdvancedStartPopCost(false, pLoopCity) > 0)
			{
				doAdvancedStartAction(ADVANCEDSTARTACTION_POP, pLoopCity->getX(), pLoopCity->getY(), -1, false);
			}
		}
	}

	if (!bNoExit)
	{
		doAdvancedStartAction(ADVANCEDSTARTACTION_EXIT, -1, -1, -1, true);
	}
}


void CvPlayerAI::AI_recalculateFoundValues(int iX, int iY, int iInnerRadius, int iOuterRadius) const
{
	PROFILE_EXTRA_FUNC();
	for (int iLoopX = -iOuterRadius; iLoopX <= iOuterRadius; iLoopX++)
	{
		for (int iLoopY = -iOuterRadius; iLoopY <= iOuterRadius; iLoopY++)
		{
			CvPlot* pLoopPlot = plotXY(iX, iY, iLoopX, iLoopY);
			if (NULL != pLoopPlot && !AI_isPlotCitySite(pLoopPlot))
			{
				if (stepDistance(0, 0, iLoopX, iLoopY) <= iInnerRadius)
				{
					if (iLoopX != 0 || iLoopY != 0)
					{
						pLoopPlot->setFoundValue(getID(), 0);
					}
				}
				else if (pLoopPlot->isRevealed(getTeam(), false))
				{
					const int iValue = AI_foundValue(pLoopPlot->getX(), pLoopPlot->getY());

					pLoopPlot->setFoundValue(getID(), iValue);

					if (iValue > pLoopPlot->area()->getBestFoundValue(getID()))
					{
						pLoopPlot->area()->setBestFoundValue(getID(), iValue);
					}
				}
			}
		}
	}
}


int CvPlayerAI::AI_getMinFoundValue() const
{
	int iValue = 4000 / (1 + getProfitMargin() * getProfitMargin());

	if (GET_TEAM(getTeam()).hasWarPlan(true))
	{
		iValue *= 2;
	}
	return iValue;
}

// returns value between 1 to 10 based on how much potential land can be gained.
int CvPlayerAI::AI_getCitySitePriorityFactor(const CvPlot* pPlot) const
{
	PROFILE_EXTRA_FUNC();
	int iPriorityCount = 1;
	const int iX = pPlot->getX();
	const int iY = pPlot->getY();

	// Toffer - No reason to check more than pPlot and the 8 surrounding plots.
	for (int iI = 0; iI < NUM_CITY_PLOTS_1; iI++)
	{
		CvPlot* plotX = plotCity(iX, iY, iI);

		if (plotX && (plotX->getTeam() != getTeam() || !plotX->isPlayerCityRadius(getID())))
		{
			iPriorityCount++;
		}
	}
	return iPriorityCount;
}

void CvPlayerAI::AI_updateCitySites(int iMinFoundValueThreshold, int iMaxSites) const
{
	PROFILE_EXTRA_FUNC();
	if (gPlayerLogLevel > 1)
	{
		logBBAI("Player %d (%S) begin Update City Sites (min. value: %d)...", getID(), getCivilizationDescription(0), iMinFoundValueThreshold);
	}
	for (int iI = 0; iI < iMaxSites; iI++)
	{
		//Add a city to the list.
		int iBestFoundValue = 0;
		CvPlot* pBestFoundPlot = NULL;

		for (int iPlot = 0; iPlot < GC.getMap().numPlots(); iPlot++)
		{
			CvPlot* plotX = GC.getMap().plotByIndex(iPlot);

			if (plotX->isRevealed(getTeam(), false) && !AI_isPlotCitySite(plotX))
			{
				const int iFoundValue = plotX->getFoundValue(getID());

				if (iFoundValue > iMinFoundValueThreshold)
				{
					const int iValue = iFoundValue * AI_getCitySitePriorityFactor(plotX);

					if (iValue > iBestFoundValue)
					{
						iBestFoundValue = iValue;
						pBestFoundPlot = plotX;
						if (gPlayerLogLevel > 1)
						{
							logBBAI("  Potential best city site (%d, %d) found value is %d (player modified value to %d)", plotX->getX(), plotX->getY(), iFoundValue, iValue);
						}
					}
				}
			}
		}
		if (pBestFoundPlot == NULL)
		{
			break;
		}
		if (gPlayerLogLevel > 1)
		{
			logBBAI("	Found City Site at (%d, %d)", pBestFoundPlot->getX(), pBestFoundPlot->getY());
		}
		m_aiAICitySites.push_back(GC.getMap().plotNum(pBestFoundPlot->getX(), pBestFoundPlot->getY()));
		AI_recalculateFoundValues(pBestFoundPlot->getX(), pBestFoundPlot->getY(), CITY_PLOTS_RADIUS, 2 * CITY_PLOTS_RADIUS);
	}
	if (gPlayerLogLevel > 1)
	{
		logBBAI("Player %d (%S) end Update City Sites", getID(), getCivilizationDescription(0));
	}
}

void CvPlayerAI::calculateCitySites() const
{
	static bool isCalulatingCitySites = false;

	//	Re-entrancy protection
	if (!isCalulatingCitySites)
	{
		isCalulatingCitySites = true;

		AI_updateFoundValues(false, NULL);

		m_bCitySitesNotCalculated = false;
		m_aiAICitySites.clear();
		AI_updateCitySites(AI_getMinFoundValue(), 4);

		isCalulatingCitySites = false;
	}
}

int CvPlayerAI::AI_getNumCitySites() const
{
	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	return m_aiAICitySites.size();
}

bool CvPlayerAI::AI_isPlotCitySite(const CvPlot* pPlot) const
{
	const int iPlotIndex = GC.getMap().plotNum(pPlot->getX(), pPlot->getY());

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	return algo::any_of_equal(m_aiAICitySites, iPlotIndex);
}

int CvPlayerAI::AI_getNumAreaCitySites(int iAreaID, int& iBestValue) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	iBestValue = 0;

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	foreach_(const int& i, m_aiAICitySites)
	{
		CvPlot* pCitySitePlot = GC.getMap().plotByIndex(i);
		if (pCitySitePlot->getArea() == iAreaID)
		{
			iCount++;
			const int iValue = pCitySitePlot->getFoundValue(getID());

			if (gPlayerLogLevel > 2)
			{
				logBBAI("Player %d (%S) - City Site %d (%d, %d) has a value of %d", getID(), getCivilizationDescription(0), iCount, pCitySitePlot->getX(), pCitySitePlot->getY(), iValue);
			}

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
			}
		}
	}
	if (gPlayerLogLevel > 1)
	{
		logBBAI("Player %d (%S) have %d City Sites, best value is %d", getID(), getCivilizationDescription(0), iCount, iBestValue);
	}
	return iCount;
}

int CvPlayerAI::AI_getNumAdjacentAreaCitySites(int iWaterAreaID, int iExcludeArea, int& iBestValue) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;
	iBestValue = 0;

	if (m_bCitySitesNotCalculated)
	{
		calculateCitySites();
	}

	foreach_(const int& i, m_aiAICitySites)
	{
		CvPlot* pCitySitePlot = GC.getMap().plotByIndex(i);
		if (pCitySitePlot->getArea() != iExcludeArea)
		{
			if (pCitySitePlot->isAdjacentToArea(iWaterAreaID))
			{
				iCount++;
				iBestValue = std::max(iBestValue, pCitySitePlot->getFoundValue(getID()));
			}
		}
	}
	return iCount;


}

CvPlot* CvPlayerAI::AI_getCitySite(int iIndex) const
{
	FASSERT_BOUNDS(0, (int)m_aiAICitySites.size(), iIndex);
	return GC.getMap().plotByIndex(m_aiAICitySites[iIndex]);
}

int CvPlayerAI::AI_bestAreaUnitAIValue(UnitAITypes eUnitAI, const CvArea* pArea, UnitTypes* peBestUnitType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_EXTRA_FUNC();
	CvCity* pCity = NULL;

	if (pArea != NULL)
	{
		if (getCapitalCity() != NULL)
		{
			if (pArea->isWater())
			{
				if (getCapitalCity()->plot()->isAdjacentToArea(pArea))
				{
					pCity = getCapitalCity();
				}
			}
			else
			{
				if (getCapitalCity()->getArea() == pArea->getID())
				{
					pCity = getCapitalCity();
				}
			}
		}

		if (NULL == pCity)
		{
			foreach_(CvCity * pLoopCity, cities())
			{
				if (pArea->isWater())
				{
					if (pLoopCity->plot()->isAdjacentToArea(pArea))
					{
						pCity = pLoopCity;
						break;
					}
				}
				else
				{
					if (pLoopCity->getArea() == pArea->getID())
					{
						pCity = pLoopCity;
						break;
					}
				}
			}
		}
	}

	return AI_bestCityUnitAIValue(eUnitAI, pCity, peBestUnitType, criteria);

}

int CvPlayerAI::AI_bestCityUnitAIValue(UnitAITypes eUnitAI, const CvCity* pCity, UnitTypes* peBestUnitType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();

	FASSERT_BOUNDS(0, NUM_UNITAI_TYPES, eUnitAI);

	int iBestValue = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const UnitTypes eLoopUnit = (UnitTypes)iI;

		if (!isHumanPlayer() || (GC.getUnitInfo(eLoopUnit).getDefaultUnitAIType() == eUnitAI))
		{
			const int iValue = AI_unitValue(eLoopUnit, eUnitAI, (pCity == NULL) ? NULL : pCity->area(), criteria);
			if (iValue > iBestValue)
			{
				if (NULL == pCity ? canTrain(eLoopUnit) : pCity->canTrain(eLoopUnit))
				{
					iBestValue = iValue;
					if (peBestUnitType != NULL)
					{
						*peBestUnitType = eLoopUnit;
					}
				}
			}
		}
	}

	return iBestValue;
}

int CvPlayerAI::AI_calculateTotalBombard(DomainTypes eDomain) const
{
	PROFILE_EXTRA_FUNC();
	int iTotalBombard = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

		if (kUnit.getDomainType() == eDomain)
		{
			const int iBombRate = kUnit.getBombRate();
			int iBombardRate = kUnit.getBombardRate() + (kUnit.getBreakdownChance() + kUnit.getBreakdownDamage() * 10) / 2;

			if (iBombardRate > 0 || iBombRate > 0)
			{
				int iNumUnits = getUnitCount((UnitTypes)iI);
				if (iBombardRate > 0)
				{
					if (kUnit.isIgnoreBuildingDefense())
					{
						iBombardRate *= 3;
						iBombardRate /= 2;
					}
					iTotalBombard += iBombardRate * iNumUnits;
				}
				if (iBombRate > 0)
				{
					iTotalBombard += iBombRate * iNumUnits;
				}
			}
		}
	}
	return iTotalBombard;
}

void CvPlayerAI::AI_updateBonusValue(BonusTypes eBonus)
{
	FAssert(m_aiBonusValue != NULL);

	//reset
	m_aiBonusValue[eBonus] = -1;
	m_abNonTradeBonusCalculated[eBonus] = false;
}


void CvPlayerAI::AI_updateBonusValue()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
	{
		AI_updateBonusValue((BonusTypes)iI);
	}
}

int CvPlayerAI::AI_getUnitWeight(UnitTypes eUnit) const
{
	return m_aiUnitWeights[eUnit] / 100;
}

int CvPlayerAI::AI_getUnitCombatWeight(UnitCombatTypes eUnitCombat) const
{
	return m_aiUnitCombatWeights[eUnitCombat] / 100;
}

void CvPlayerAI::AI_doEnemyUnitData()
{
	PROFILE_FUNC();

	std::vector<int> aiUnitCounts(GC.getNumUnitInfos(), 0);

	std::vector<int> aiDomainSums(NUM_DOMAIN_TYPES, 0);

	int iI;

	int iNewTotal = 0;

	// Count enemy land and sea units visible to us
	for (iI = 0; iI < GC.getMap().numPlots(); iI++)
	{
		const CvPlot* pLoopPlot = GC.getMap().plotByIndex(iI);
		int iAdjacentAttackers = -1;
		if (pLoopPlot->isVisible(getTeam(), false))
		{
			foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
			{
				if (pLoopUnit->canFight())
				{
					int iUnitValue = 1;

					if (GET_TEAM(getTeam()).AI_getWarPlan(pLoopUnit->getTeam()) != NO_WARPLAN)
					{
						iUnitValue += 10;

						if ((pLoopPlot->getOwner() == getID()))
						{
							iUnitValue += 15;
						}
						else if (atWar(getTeam(), pLoopPlot->getTeam()))
						{
							if (iAdjacentAttackers == -1)
							{
								iAdjacentAttackers = GET_PLAYER(pLoopPlot->getOwner()).AI_adjacentPotentialAttackers(pLoopPlot);
							}
							if (iAdjacentAttackers > 0)
							{
								iUnitValue += 15;
							}
						}
					}

					else if (pLoopUnit->getTeam() != getTeam())
					{
						iUnitValue += pLoopUnit->canAttack() ? 4 : 1;
						if (pLoopPlot->getCulture(getID()) > 0)
						{
							iUnitValue += pLoopUnit->canAttack() ? 4 : 1;
						}
					}

					// If we hadn't seen any of this class before
					if (m_aiUnitWeights[pLoopUnit->getUnitType()] == 0)
					{
						iUnitValue *= 4;
					}

					iUnitValue *= pLoopUnit->baseCombatStrNonGranular();
					aiUnitCounts[pLoopUnit->getUnitType()] += iUnitValue;
					aiDomainSums[pLoopUnit->getDomainType()] += iUnitValue;
					iNewTotal += iUnitValue;
				}
			}
		}
	}

	if (iNewTotal == 0)
	{
		//This should rarely happen.
		return;
	}

	//Decay
	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		m_aiUnitWeights[iI] -= 100;
		m_aiUnitWeights[iI] *= 3;
		m_aiUnitWeights[iI] /= 4;
		m_aiUnitWeights[iI] = std::max(0, m_aiUnitWeights[iI]);
	}

	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (aiUnitCounts[iI] > 0)
		{
			TechTypes eTech = (TechTypes)GC.getUnitInfo((UnitTypes)iI).getPrereqAndTech();

			int iEraDiff = (eTech == NO_TECH) ? 4 : std::min(4, getCurrentEra() - GC.getTechInfo(eTech).getEra());

			if (iEraDiff > 1)
			{
				iEraDiff -= 1;
				aiUnitCounts[iI] *= 3 - iEraDiff;
				aiUnitCounts[iI] /= 3;
			}
			FAssert(aiDomainSums[GC.getUnitInfo((UnitTypes)iI).getDomainType()] > 0);
			m_aiUnitWeights[iI] += (5000 * aiUnitCounts[iI]) / std::max(1, aiDomainSums[GC.getUnitInfo((UnitTypes)iI).getDomainType()]);
		}
	}

	for (iI = 0; iI < GC.getNumUnitCombatInfos(); ++iI)
	{
		m_aiUnitCombatWeights[iI] = 0;
	}

	for (iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (m_aiUnitWeights[iI] > 0)
		{
			int ctype = GC.getUnitInfo((UnitTypes)iI).getUnitCombatType();
			if (ctype >= 0 && ctype < GC.getNumUnitCombatInfos())
			{
				m_aiUnitCombatWeights[ctype] += m_aiUnitWeights[iI];
			}
		}
	}

	for (iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		if (m_aiUnitCombatWeights[iI] > 2500)
		{
			m_aiUnitCombatWeights[iI] += 2500;
		}
		else if (m_aiUnitCombatWeights[iI] > 0)
		{
			m_aiUnitCombatWeights[iI] += 1000;
		}
	}
}

int CvPlayerAI::AI_calculateUnitAIViability(UnitAITypes eUnitAI, DomainTypes eDomain) const
{
	PROFILE_EXTRA_FUNC();
	int iBestUnitAIStrength = 0;
	int iBestOtherStrength = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		const CvUnitInfo& kUnitInfo = GC.getUnitInfo((UnitTypes)iI);

		if (kUnitInfo.getDomainType() == eDomain)
		{
			TechTypes eTech = (TechTypes)kUnitInfo.getPrereqAndTech();
			if (m_aiUnitWeights[iI] > 0 || GET_TEAM(getTeam()).isHasTech((eTech)))
			{
				if (kUnitInfo.getUnitAIType(eUnitAI))
				{
					iBestUnitAIStrength = std::max(iBestUnitAIStrength, kUnitInfo.getCombat());
				}
				iBestOtherStrength = std::max(iBestOtherStrength, kUnitInfo.getCombat());
			}
		}
	}
	return (100 * iBestUnitAIStrength) / std::max(1, iBestOtherStrength);
}

ReligionTypes CvPlayerAI::AI_chooseReligion()
{
	PROFILE_EXTRA_FUNC();
	const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
	if (NO_RELIGION != eFavorite && !GC.getGame().isReligionFounded(eFavorite))
	{
		return eFavorite;
	}

	std::vector<ReligionTypes> aeReligions;
	for (int iReligion = 0; iReligion < GC.getNumReligionInfos(); ++iReligion)
	{
		if (!GC.getGame().isReligionFounded((ReligionTypes)iReligion))
		{
			aeReligions.push_back((ReligionTypes)iReligion);
		}
	}

	if (!aeReligions.empty())
	{
		return aeReligions[GC.getGame().getSorenRandNum(aeReligions.size(), "AI pick religion")];
	}

	return NO_RELIGION;
}

int CvPlayerAI::AI_getAttitudeWeight(PlayerTypes ePlayer) const
{
	int iAttitudeWeight = 0;
	switch (AI_getAttitude(ePlayer))
	{
	case ATTITUDE_FURIOUS:
		iAttitudeWeight = -100;
		break;
	case ATTITUDE_ANNOYED:
		iAttitudeWeight = -50;
		break;
	case ATTITUDE_CAUTIOUS:
		iAttitudeWeight = 0;
		break;
	case ATTITUDE_PLEASED:
		iAttitudeWeight = 50;
		break;
	case ATTITUDE_FRIENDLY:
		iAttitudeWeight = 100;
		break;
	}

	return iAttitudeWeight;
}

int CvPlayerAI::AI_getPlotAirbaseValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	if (pPlot->isOwned() && (pPlot->getTeam() != getTeam()))
	{
		return 0;
	}

	if (pPlot->isCityRadius())
	{
		CvCity* pWorkingCity = pPlot->getWorkingCity();
		if (pWorkingCity != NULL)
		{
			if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
			{
				return 0;
			}
			if (pPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				if (!GC.getImprovementInfo(pPlot->getImprovementType()).isActsAsCity())
				{
					return 0;
				}
			}
		}
	}

	int iMinOtherCityDistance = MAX_INT;
	const CvPlot* iMinOtherCityPlot = NULL;

	// Super Forts begin *choke* *canal* - commenting out unnecessary code
	//	int iMinFriendlyCityDistance = MAX_INT;
	//	CvPlot* iMinFriendlyCityPlot = NULL;

	int iOtherCityCount = 0;

	int iRange = 4;
	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange))
	{
		if (pPlot != pLoopPlot)
		{
			const int iDistance = plotDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY());

			if (pLoopPlot->getTeam() == getTeam())
			{
				if (pLoopPlot->isCity(true))
				{
					if (1 == iDistance)
					{
						return 0;
					}
					//					if (iDistance < iMinFriendlyCityDistance)
					//					{
					//						iMinFriendlyCityDistance = iDistance;
					//						iMinFriendlyCityPlot = pLoopPlot;
					//					}
					// Super Forts end
				}
			}
			else if (pLoopPlot->isOwned() && pLoopPlot->isCity(false))
			{
				if (iDistance < iMinOtherCityDistance)
				{
					iMinOtherCityDistance = iDistance;
					iMinOtherCityPlot = pLoopPlot;
					// Super Forts begin  *choke* *canal* - move iOtherCityCount outside the if statement
				}
				iOtherCityCount++;
				// Super Forts end
			}
		}
	}

	if (0 == iOtherCityCount)
	{
		return 0;
	}

	//	if (iMinFriendlyCityPlot != NULL)
	//	{
	//		FAssert(iMinOtherCityPlot != NULL);
	//		if (plotDistance(iMinFriendlyCityPlot->getX(), iMinFriendlyCityPlot->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < (iMinOtherCityDistance - 1))
	//		{
	//			return 0;
	//		}
	//	}

	//	if (iMinOtherCityPlot != NULL)
	//	{
	//		CvCity* pNearestCity = GC.getMap().findCity(iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY(), NO_PLAYER, getTeam(), false);
	//		if (NULL == pNearestCity)
	//		{
	//			return 0;
	//		}
	//		if (plotDistance(pNearestCity->getX(), pNearestCity->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < iRange)
	//		{
	//			return 0;
	//		}
	//	}


		// Super Forts begin *canal* *choke*
	if (iOtherCityCount == 1)
	{
		if (iMinOtherCityPlot)
		{
			CvCity* pNearestCity = GC.getMap().findCity(iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY(), NO_PLAYER, getTeam(), false);
			if (NULL != pNearestCity)
			{
				if (plotDistance(pNearestCity->getX(), pNearestCity->getY(), iMinOtherCityPlot->getX(), iMinOtherCityPlot->getY()) < iMinOtherCityDistance)
				{
					return 0;
				}
			}
		}
	}
	const int iValue = iOtherCityCount * 50 + pPlot->defenseModifier(getTeam(), false);
	// Super Forts end

	return std::max(0, iValue);
}

int CvPlayerAI::AI_getPlotCanalValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	// Super Forts begin *canal*
	int iCanalValue = pPlot->getCanalValue();

	if (iCanalValue > 0)
	{
		if (pPlot->isOwned())
		{
			if (pPlot->getTeam() != getTeam())
			{
				return 0;
			}
			if (pPlot->isCityRadius())
			{
				CvCity* pWorkingCity = pPlot->getWorkingCity();
				if (pWorkingCity != NULL)
				{
					// Left in this part from the original code. Might be needed to avoid workers from getting stuck in a loop?
					if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
					{
						return 0;
					}
					if (pPlot->getImprovementType() != NO_IMPROVEMENT)
					{
						if (!GC.getImprovementInfo(pPlot->getImprovementType()).isActsAsCity())
						{
							return 0;
						}
					}
					// Decrease value when within radius of a city
					iCanalValue -= 5;
				}
			}
		}

		foreach_(const CvPlot * pLoopPlot, pPlot->adjacent())
		{
			if (pLoopPlot->isCity(true) && (pLoopPlot->getCanalValue() > 0))
			{
				// Decrease value when adjacent to a city or fort with a canal value
				iCanalValue -= 10;
			}
		}

		iCanalValue *= 10;
		// Favor plots with higher defense
		int iDefenseModifier = pPlot->defenseModifier(getTeam(), false);
		iCanalValue += iDefenseModifier;
	}

	return std::max(0, iCanalValue);
	// Super Forts end
}

// Super Forts begin *choke*
int CvPlayerAI::AI_getPlotChokeValue(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	FAssert(pPlot != NULL);

	int iChokeValue = pPlot->getChokeValue();

	if (iChokeValue > 0)
	{
		if (pPlot->isOwned())
		{
			if (pPlot->getTeam() != getTeam())
			{
				return 0;
			}
			if (pPlot->isCityRadius())
			{
				CvCity* pWorkingCity = pPlot->getWorkingCity();
				if (pWorkingCity != NULL)
				{
					// Left in this part from the original code. Might be needed to avoid workers from getting stuck in a loop?
					if (pWorkingCity->AI_getBestBuild(pWorkingCity->getCityPlotIndex(pPlot)) != NO_BUILD)
					{
						return 0;
					}
					if (pPlot->getImprovementType() != NO_IMPROVEMENT)
					{
						if (!GC.getImprovementInfo(pPlot->getImprovementType()).isMilitaryStructure())
						{
							return 0;
						}
					}
					// Decrease value when within radius of a city
					iChokeValue -= 5;
				}
			}
		}

		foreach_(const CvPlot * pLoopPlot, pPlot->adjacent())
		{
			if (pLoopPlot->isCity(true) && (pLoopPlot->getChokeValue() > 0))
			{
				// Decrease value when adjacent to a city or fort with a choke value
				iChokeValue -= 10;
			}
		}

		iChokeValue *= 10;
		// Favor plots with higher defense
		int iDefenseModifier = pPlot->defenseModifier(getTeam(), false);
		iChokeValue += iDefenseModifier;
	}

	return std::max(0, iChokeValue);
}
// Super Forts end

bool CvPlayerAI::AI_isCivicCanChangeOtherValues(CivicTypes eCivicSelected, ReligionTypes eAssumedReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (eCivicSelected == NO_CIVIC)
	{
		return false;
	}

	const CvCivicInfo& kCivicSelected = GC.getCivicInfo(eCivicSelected);

	//happiness
	if (kCivicSelected.getCivicPercentAnger() != 0 || kCivicSelected.getHappyPerMilitaryUnit() != 0
	|| kCivicSelected.getLargestCityHappiness() != 0 || kCivicSelected.getNonStateReligionHappiness() != 0
	|| kCivicSelected.isAnyBuildingHappinessChange() || kCivicSelected.isAnyFeatureHappinessChange()
	|| (kCivicSelected.getWarWearinessModifier() != 0 && getWarWearinessPercentAnger() != 0)
	|| (kCivicSelected.getStateReligionHappiness() != 0 && (kCivicSelected.isStateReligion() || eAssumedReligion != NO_RELIGION)))
	{
		return true;
	}

	//health
	if (kCivicSelected.getExtraHealth() != 0 || kCivicSelected.isNoUnhealthyPopulation() || kCivicSelected.isBuildingOnlyHealthy() || kCivicSelected.isAnyBuildingHealthChange())
	{
		return true;
	}

	//trade
	if (kCivicSelected.isNoForeignTrade())
	{
		return true;
	}

	//corporation
	if (kCivicSelected.isNoCorporations() || kCivicSelected.isNoForeignCorporations() || kCivicSelected.getCorporationMaintenanceModifier() != 0)
	{
		return true;
	}

	//religion
	if (kCivicSelected.isStateReligion())
	{
		return true;
	}
	if (kCivicSelected.isNoNonStateReligionSpread())
	{
		return true;
	}

	//other
	if (kCivicSelected.isMilitaryFoodProduction())
	{
		return true;
	}

	int iI;
	for (iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (kCivicSelected.isHurry(iI))
		{
			return true;
		}
	}
	for (iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (kCivicSelected.isSpecialBuildingNotRequired(iI))
		{
			return true;
		}
	}

	return false;
}

bool CvPlayerAI::AI_isCivicValueRecalculationRequired(CivicTypes eCivic, CivicTypes eCivicSelected, ReligionTypes eAssumedReligion) const
{
	PROFILE_EXTRA_FUNC();
	if (eCivicSelected == NO_CIVIC || eCivic == NO_CIVIC)
	{
		return false;
	}

	const CvCivicInfo& kCivic = GC.getCivicInfo(eCivic);
	const CvCivicInfo& kCivicSelected = GC.getCivicInfo(eCivicSelected);

	//happiness
	if (kCivic.getCivicPercentAnger() != 0 || kCivic.getHappyPerMilitaryUnit() != 0
	|| kCivic.getLargestCityHappiness() != 0 || kCivic.getNonStateReligionHappiness() != 0
	|| kCivic.isAnyBuildingHappinessChange() || kCivic.isAnyFeatureHappinessChange()
	|| (kCivic.getWarWearinessModifier() != 0 && getWarWearinessPercentAnger() != 0)
	|| (kCivic.getStateReligionHappiness() != 0 && (kCivic.isStateReligion() || eAssumedReligion != NO_RELIGION)))
	{
		if (kCivicSelected.getCivicPercentAnger() != 0 || kCivicSelected.getHappyPerMilitaryUnit() != 0
		|| kCivicSelected.getLargestCityHappiness() != 0 || kCivicSelected.getNonStateReligionHappiness() != 0
		|| kCivicSelected.isAnyBuildingHappinessChange() || kCivicSelected.isAnyFeatureHappinessChange()
		|| (kCivicSelected.getWarWearinessModifier() != 0 && getWarWearinessPercentAnger() != 0)
		|| (kCivicSelected.getStateReligionHappiness() != 0 && (kCivicSelected.isStateReligion() || eAssumedReligion != NO_RELIGION)))
		{
			return true;
		}
	}

	//health
	if (kCivic.getExtraHealth() != 0 || kCivic.isNoUnhealthyPopulation() || kCivic.isBuildingOnlyHealthy() || kCivic.isAnyBuildingHealthChange())
	{
		if (kCivicSelected.getExtraHealth() != 0 || kCivicSelected.isNoUnhealthyPopulation() || kCivicSelected.isBuildingOnlyHealthy() || kCivicSelected.isAnyBuildingHealthChange())
		{
			return true;
		}
	}

	//trade
	if (kCivic.isNoForeignTrade())
	{
		if (kCivicSelected.isNoForeignTrade())
		{
			return true;
		}
	}

	//corporation
	if (kCivic.isNoCorporations() || kCivic.isNoForeignCorporations() || kCivic.getCorporationMaintenanceModifier() != 0)
	{
		if (kCivicSelected.isNoCorporations() || kCivicSelected.isNoForeignCorporations() || kCivicSelected.getCorporationMaintenanceModifier() != 0)
		{
			return true;
		}
	}

	//religion
	if (kCivic.isStateReligion())
	{
		if (kCivicSelected.isStateReligion())
		{
			return true;
		}
		if (kCivic.isNoNonStateReligionSpread() && kCivicSelected.isNoNonStateReligionSpread())
		{
			return true;
		}
	}
	else
	{
		if (kCivicSelected.isStateReligion())
		{
			if (kCivic.isNoNonStateReligionSpread())
			{
				return true;
			}
			if (getStateReligionCount() > 1)
			{
				if ((kCivic.getStateReligionHappiness() != 0 && kCivic.getStateReligionHappiness() != kCivic.getNonStateReligionHappiness())
					|| kCivic.getStateReligionGreatPeopleRateModifier() != 0 || kCivic.getStateReligionUnitProductionModifier() != 0
					|| kCivic.getStateReligionBuildingProductionModifier() != 0 || kCivic.getStateReligionFreeExperience() != 0)
				{
					return true;
				}
			}
		}
	}

	//other kCivicSelected
	if (kCivic.isMilitaryFoodProduction() && kCivicSelected.isMilitaryFoodProduction())
	{
		return true;
	}

	int iI;
	for (iI = 0; iI < GC.getNumHurryInfos(); iI++)
	{
		if (kCivic.isHurry(iI) && kCivicSelected.isHurry(iI))
		{
			return true;
		}
	}
	for (iI = 0; iI < GC.getNumSpecialBuildingInfos(); iI++)
	{
		if (kCivic.isSpecialBuildingNotRequired(iI) && kCivicSelected.isSpecialBuildingNotRequired(iI))
		{
			return true;
		}
	}

	return false;
}

//This returns a positive number equal approximately to the sum
//of the percentage values of each unit (there is no need to scale the output by iHappy)
//100 * iHappy means a high value.
int CvPlayerAI::AI_getHappinessWeight(int iHappy, int iExtraPop) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	if (0 == iHappy)
	{
		iHappy = 1;
	}
	int iValue = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		const int iCityHappy = pLoopCity->happyLevel() - pLoopCity->unhappyLevel(iExtraPop) - std::max(0, pLoopCity->getCommerceHappiness());

		//Fuyu: max happy 5
		const int iHappyNow = std::min(5, iCityHappy);
		const int iHappyThen = std::min(5, iCityHappy + iHappy);

		//Integration
		int iTempValue = ((100 * iHappyThen - 10 * iHappyThen * iHappyThen) - (100 * iHappyNow - 10 * iHappyNow * iHappyNow));

		if (pLoopCity->isCapital() && isNoCapitalUnhappiness())
		{
			iTempValue /= 3;
		}

		if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_REVOLUTION))
		{
			iTempValue *= 3;
			iTempValue /= 2;
		}

		if (iHappy > 0)
		{
			iValue += std::max(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}
		else
		{
			iValue += std::min(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}

		iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
	}
	return (0 == iCount) ? 50 * iHappy : iValue / iCount;
}

int CvPlayerAI::AI_getHealthWeight(int iHealth, int iExtraPop) const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	if (0 == iHealth)
	{
		iHealth = 1;
	}
	int iValue = 0;
	foreach_(const CvCity * pLoopCity, cities())
	{
		int iCityHealth = pLoopCity->goodHealth() - pLoopCity->badHealth(false, iExtraPop);

		//Fuyu: max health 8
		int iHealthNow = std::min(8, iCityHealth);
		int iHealthThen = std::min(8, iCityHealth + iHealth);

		int iTempValue = ((100 * iHealthThen - 6 * iHealthThen * iHealthThen) - (100 * iHealthNow - 6 * iHealthNow * iHealthNow));
		if (iHealth > 0)
		{
			iValue += std::max(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}
		else
		{
			iValue += std::min(0, iTempValue) * (pLoopCity->getPopulation() + iExtraPop + 2);
		}

		iCount += (pLoopCity->getPopulation() + iExtraPop + 2);
	}

	return (0 == iCount) ? 50 * iHealth : iValue / iCount;
}

void CvPlayerAI::AI_invalidateCloseBordersAttitudeCache()
{
	PROFILE_EXTRA_FUNC();
	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		m_aiCloseBordersAttitudeCache[i] = MAX_INT;

		AI_invalidateAttitudeCache((PlayerTypes)i);
	}
}

bool CvPlayerAI::AI_isPlotThreatened(const CvPlot* pPlot, int iRange, bool bTestMoves) const
{
	PROFILE_FUNC();

	const CvArea* pPlotArea = pPlot->area();

	if (iRange == -1)
	{
		iRange = DANGER_RANGE;
	}

	foreach_(const CvPlot * pLoopPlot, pPlot->rect(iRange, iRange)
	| filtered(CvPlot::fn::area() == pPlotArea))
	{
		CvSelectionGroup* pLoopGroup = NULL;

		do
		{
			CvSelectionGroup* pNextGroup = NULL;

			foreach_(const CvUnit * pLoopUnit, pLoopPlot->units())
			{
				if (pLoopUnit->isEnemy(getTeam()) && pLoopUnit->canAttack() && !pLoopUnit->isInvisible(getTeam(), false))
				{
					if (pLoopGroup == NULL ||
						 pLoopUnit->getOwner() > pLoopGroup->getOwner() ||
						 (pLoopUnit->getOwner() == pLoopGroup->getOwner() && pLoopUnit->getGroupID() > pLoopGroup->getID()))
					{
						if (pNextGroup == NULL ||
							 pLoopUnit->getOwner() < pNextGroup->getOwner() ||
							 (pLoopUnit->getOwner() == pNextGroup->getOwner() && pLoopUnit->getGroupID() < pNextGroup->getID()))
						{
							pNextGroup = pLoopUnit->getGroup();
						}
					}
				}
			}

			pLoopGroup = pNextGroup;

			if (pLoopGroup != NULL && pLoopGroup->canEnterOrAttackPlot(pPlot))
			{
				int iPathTurns = 0;
				if (bTestMoves)
				{
					iPathTurns = pLoopGroup->canPathDirectlyTo(pLoopPlot, pPlot) ? 1 : MAX_INT;
					//if (!pLoopUnit->getGroup()->generatePath(pLoopPlot, pPlot, MOVE_MAX_MOVES | MOVE_IGNORE_DANGER, false, &iPathTurns))
					//{
					//	iPathTurns = MAX_INT;
					//}
				}

				if (iPathTurns <= 1)
				{
					return true;
				}
			}
		} while (pLoopGroup != NULL);
	}

	return false;
}

bool CvPlayerAI::AI_isFirstTech(TechTypes eTech) const
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (GC.getReligionInfo((ReligionTypes)iI).getTechPrereq() == eTech
		&& !GC.getGame().isReligionSlotTaken((ReligionTypes)iI)
		&& canFoundReligion())
		{
			return true;
		}
	}
	if (GC.getGame().countKnownTechNumTeams(eTech) == 0
	&& ((UnitTypes)GC.getTechInfo(eTech).getFirstFreeUnit() != NO_UNIT || GC.getTechInfo(eTech).getFirstFreeTechs() > 0))
	{
		return true;
	}
	return false;
}

void CvPlayerAI::AI_invalidateAttitudeCache(PlayerTypes ePlayer)
{
	m_aiAttitudeCache[ePlayer] = MAX_INT;
}

void CvPlayerAI::AI_invalidateAttitudeCache()
{
	PROFILE_EXTRA_FUNC();
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		AI_invalidateAttitudeCache((PlayerTypes)iI);
	}
}

void CvPlayerAI::AI_changeAttitudeCache(const PlayerTypes ePlayer, const int iChange)
{
	if (m_aiAttitudeCache[ePlayer] == 100 && iChange < 0
	|| m_aiAttitudeCache[ePlayer] == -100 && iChange > 0)
	{
		// Toffer - Why don't we allow the total value to exceed +/- 100 ?
		AI_invalidateAttitudeCache(ePlayer);
	}
	else m_aiAttitudeCache[ePlayer] = range(m_aiAttitudeCache[ePlayer] + iChange, -100, 100);
}

CvCity* CvPlayerAI::getInquisitionRevoltCity(const CvUnit* pUnit, const bool bNoUnit, int iRevIndexThreshold, const int iTrendThreshold) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);
	if (!(hasInquisitionTarget()))
	{
		return NULL;
	}

	CvCity* pBestCity = NULL;
	const CvPlot* pUnitPlot = pUnit->plot();
	int iBestRevoltIndex = 100;

	foreach_(CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isInquisitionConditions())
		{
			if ((pLoopCity->getRevTrend() > iTrendThreshold)
			|| (pLoopCity->getRevolutionIndex() > 1000))
			{
				int iTempCityValue = pLoopCity->getRevolutionIndex() + 7 * (pLoopCity->getRevTrend());
				iTempCityValue -= 10 * (pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopCity->plot()));
				if (iTempCityValue > iBestRevoltIndex)
				{
					if ((bNoUnit) || (pUnit->generatePath(pLoopCity->plot(), 0, false)))
					{
						iBestRevoltIndex = iTempCityValue;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}

	return pBestCity;
}

CvCity* CvPlayerAI::getTeamInquisitionRevoltCity(const CvUnit* pUnit, const bool bNoUnit, int iRevIndexThreshold, const int iTrendThreshold) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);
	if (!(hasInquisitionTarget()))
	{
		return NULL;
	}

	CvCity* pBestCity = NULL;
	const CvPlot* pUnitPlot = pUnit->plot();
	int iBestRevoltIndex = 100;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		if (kLoopPlayer.isAlive())
		{
			if ((kLoopPlayer.getTeam() == getTeam()) ||
			(GET_TEAM(getTeam()).isVassal((TeamTypes)kLoopPlayer.getTeam())))
			{
				foreach_(CvCity * pLoopCity, kLoopPlayer.cities())
				{
					if (pLoopCity->isInquisitionConditions())
					{
						if ((pLoopCity->getRevTrend() > iTrendThreshold)
						|| (pLoopCity->getRevolutionIndex() > 1000))
						{
							int iTempCityValue = pLoopCity->getRevolutionIndex() + 7 * (pLoopCity->getRevTrend());
							iTempCityValue -= 10 * (pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopCity->plot()));
							if (iTempCityValue > iBestRevoltIndex)
							{
								if ((bNoUnit) || (pUnit->generatePath(pLoopCity->plot(), 0, false)))
								{
									iBestRevoltIndex = iTempCityValue;
									pBestCity = pLoopCity;
								}
							}
						}
					}
				}
			}
		}
	}

	return pBestCity;
}

CvCity* CvPlayerAI::getReligiousVictoryTarget(const CvUnit* pUnit, const bool bNoUnit) const
{
	PROFILE_EXTRA_FUNC();
	FAssert(pUnit != NULL);

	if (!hasInquisitionTarget() || (!isPushReligiousVictory() && !isConsiderReligiousVictory()))
	{
		return NULL;
	}

	const CvPlot* pUnitPlot = pUnit->plot();
	const ReligionTypes eStateReligion = getStateReligion();

	CvCity* pBestCity = NULL;
	int iBestCityValue = MAX_INT;
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		const CvTeam& kLoopTeam = GET_TEAM(kLoopPlayer.getTeam());

		if (kLoopPlayer.isAlive()
			&& (TeamTypes(kLoopPlayer.getTeam()) == getTeam() || kLoopTeam.isVassal((TeamTypes)kLoopPlayer.getTeam()))
			&& pUnitPlot->isHasPathToPlayerCity(getTeam(), PlayerTypes(iI))
			)
		{
			foreach_(CvCity * pLoopCity, kLoopPlayer.cities())
			{
				CvPlot* pLoopPlot = pLoopCity->plot();
				if (pLoopCity->isInquisitionConditions()
					&& (bNoUnit || pUnit->generatePath(pLoopPlot, 0, false))
					)
				{
					int tempCityValue = pUnitPlot->calculatePathDistanceToPlot(getTeam(), pLoopPlot);
					if (isNonStateReligionCommerce()
						&& kLoopPlayer.getID() == getID())
					{
						tempCityValue *= 2;
					}
					if (kLoopTeam.isVassal((TeamTypes)kLoopPlayer.getTeam()))
					{
						tempCityValue -= 12;
					}
					for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
					{
						const ReligionTypes religionType = static_cast<ReligionTypes>(iJ);
						if (religionType != eStateReligion
							&& hasHolyCity(religionType)
							&& pLoopCity->isHasReligion(religionType))
						{
							tempCityValue += 13;
						}
					}
					if (tempCityValue < iBestCityValue)
					{
						pBestCity = pLoopCity;
						iBestCityValue = tempCityValue;
					}
				}
			}
		}
	}

	return pBestCity;
}


bool CvPlayerAI::isPushReligiousVictory() const
{
	return m_bPushReligiousVictory;
}

void CvPlayerAI::AI_setPushReligiousVictory()
{
	PROFILE_FUNC();

	m_bPushReligiousVictory = false;
	const ReligionTypes eStateReligion = getStateReligion();

	if (eStateReligion == NO_RELIGION
	|| !hasHolyCity(eStateReligion)
	|| AI_getCultureVictoryStage() > 1)
	{
		return;
	}

	// Better way to determine if religious victory is valid?
	int iVictoryTarget;
	bool bValid = false;
	for (int iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.getReligionPercent() > 0)
			{
				iVictoryTarget = kVictoryInfo.getReligionPercent();
				bValid = true;
				break;
			}
		}
	}
	if (!bValid)
	{
		m_bPushReligiousVictory = false;
		return;
	}
	const int iStateReligionInfluence = GC.getGame().calculateReligionPercent(eStateReligion);

	if (iStateReligionInfluence > (3 * iVictoryTarget) / 4)
	{
		m_bPushReligiousVictory = true;
		return;
	}

	bool bStateReligionBest = true;
	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (eStateReligion != (ReligionTypes)iI)
		{
			if (GC.getGame().calculateReligionPercent((ReligionTypes)iI) > iStateReligionInfluence)
			{
				bStateReligionBest = false;
				break;
			}
		}
	}

	int iPercentThreshold = iVictoryTarget * 2/3;

	if (eStateReligion != NO_RELIGION)
	{
		const CvCity* holyCity = GC.getGame().getHolyCity(eStateReligion);

		if (holyCity && holyCity->getOwner() == getID())
		{
			foreach_(const BuildingTypes eTypeX, holyCity->getHasBuildings())
			{
				if (GC.getBuildingInfo(eTypeX).getGlobalReligionCommerce() == eStateReligion
				&& !holyCity->isDisabledBuilding(eTypeX))
				{
					iPercentThreshold /= 2;
					break;
				}
			}
		}
	}

	if (bStateReligionBest
	&& (iStateReligionInfluence > iPercentThreshold || GET_TEAM(getTeam()).getTotalLand(true) > 50))
	{
		m_bPushReligiousVictory = true;
	}
}


bool CvPlayerAI::isConsiderReligiousVictory() const
{
	return m_bConsiderReligiousVictory;
}

void CvPlayerAI::AI_setConsiderReligiousVictory()
{
	PROFILE_FUNC();

	if (isPushReligiousVictory())
	{
		m_bConsiderReligiousVictory = true;
		return;
	}

	m_bConsiderReligiousVictory = false;
	if (getStateReligion() == NO_RELIGION)
	{
		return;
	}
	if (AI_getCultureVictoryStage() > 1)
	{
		return;
	}

	ReligionTypes eStateReligion = getStateReligion();
	int iStateReligionInfluence = GC.getGame().calculateReligionPercent(eStateReligion);
	int iI;
	int iVictoryTarget;

	if (!hasHolyCity(eStateReligion))
	{
		return;
	}
	// Better way to determine if religious victory is valid?
	bool bValid = false;
	for (iI = 0; iI < GC.getNumVictoryInfos(); iI++)
	{
		if (GC.getGame().isVictoryValid((VictoryTypes)iI))
		{
			const CvVictoryInfo& kVictoryInfo = GC.getVictoryInfo((VictoryTypes)iI);
			if (kVictoryInfo.getReligionPercent() > 0)
			{
				iVictoryTarget = kVictoryInfo.getReligionPercent();
				bValid = true;
				break;
			}
		}
	}
	if (!bValid)
	{
		return;
	}

	if (iStateReligionInfluence > ((2 * iVictoryTarget) / 3))
	{
		m_bConsiderReligiousVictory = true;
		return;
	}

	bool eStateReligionBest = true;
	for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
	{
		if (eStateReligion != (ReligionTypes)iI)
		{
			if (GC.getGame().calculateReligionPercent((ReligionTypes)iI) > iStateReligionInfluence)
			{
				eStateReligionBest = false;
				break;
			}
		}
	}
	if (eStateReligionBest)
	{
		m_bConsiderReligiousVictory = true;
		return;
	}
	else
	{
		if ((iStateReligionInfluence > (iVictoryTarget / 2)))
		{
			m_bConsiderReligiousVictory = true;
			return;
		}
	}

}


bool CvPlayerAI::hasInquisitionTarget() const
{
	return m_bHasInquisitionTarget;
}

void CvPlayerAI::AI_setHasInquisitionTarget()
{
	PROFILE_FUNC();

	m_bHasInquisitionTarget = false;
	if (!isInquisitionConditions())
	{
		return;
	}

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
		if (kLoopPlayer.isAlive())
		{
			if ((kLoopPlayer.getTeam() == getTeam()) ||
			(GET_TEAM(getTeam()).isVassal((TeamTypes)kLoopPlayer.getTeam())))
			{
				foreach_(const CvCity * pLoopCity, kLoopPlayer.cities())
				{
					if (pLoopCity->isInquisitionConditions())
					{
						if ((pLoopCity->getRevTrend() > 0) && (pLoopCity->getRevolutionIndex() > 500))
						{
							m_bHasInquisitionTarget = true;
							return;
						}
						else if (pLoopCity->getRevolutionIndex() > 1000)
						{
							m_bHasInquisitionTarget = true;
							return;
						}
					}
				}
			}
		}
	}

	if (isPushReligiousVictory() || isConsiderReligiousVictory())
	{
		for (int iI = 0; iI < MAX_PLAYERS; iI++)
		{
			const CvPlayer& kLoopPlayer = GET_PLAYER(PlayerTypes(iI));
			if (kLoopPlayer.isAlive())
			{
				const CvTeam& kLoopTeam = GET_TEAM(kLoopPlayer.getTeam());
				if ((TeamTypes(kLoopPlayer.getTeam()) == getTeam()) || kLoopTeam.isVassal((TeamTypes)kLoopPlayer.getTeam()))
				{
					foreach_(const CvCity * pLoopCity, kLoopPlayer.cities())
					{
						if (pLoopCity->isInquisitionConditions())
						{
							m_bHasInquisitionTarget = true;
							return;
						}
					}
				}
			}
		}
	}
}

int CvPlayerAI::countCityReligionRevolts() const
{
	PROFILE_EXTRA_FUNC();
	int iCount = 0;

	foreach_(const CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isInquisitionConditions())
		{
			if ((pLoopCity->getRevTrend() > 0) && (pLoopCity->getRevolutionIndex() > 500))
			{
				iCount++;
			}
			else if (pLoopCity->getRevolutionIndex() > 1000)
			{
				iCount++;
			}
		}
	}

	return iCount;
}


int CvPlayerAI::AI_getEmbassyAttitude(PlayerTypes ePlayer) const
{
	PROFILE_FUNC();

	const bool bVictim = ((GET_TEAM(getTeam()).AI_getMemoryCount((GET_PLAYER(ePlayer).getTeam()), MEMORY_DECLARED_WAR) == 0) && (GET_TEAM(getTeam()).isAtWar(GET_PLAYER(ePlayer).getTeam())));

	int iAttitude = 0;

	if (GET_TEAM(getTeam()).isHasEmbassy(GET_PLAYER(ePlayer).getTeam()))
	{
		iAttitude = 1;
	}
	else if (((GET_TEAM(getTeam())).AI_getMemoryCount(GET_PLAYER(ePlayer).getTeam(), MEMORY_RECALLED_AMBASSADOR) > 0) && !bVictim)
	{
		iAttitude = -2;
	}

	return iAttitude;
}

int CvPlayerAI::AI_workerTradeVal(const CvUnit* pUnit) const
{
	PROFILE_FUNC();


	if (!GC.getUnitInfo(pUnit->getUnitType()).isWorkerTrade())
	{//It's not a worker, so it's worthless
		return 0;
	}

	//	Also scale by relative merits of this unit compared to the best worker we can produce in our capital
	UnitTypes eBestWorker = NO_UNIT;

	if (getCapitalCity() != NULL)
	{
		int iDummyValue;

		//	This can be called synchronmously orm asynchronously so set to no-rand
		eBestWorker = getCapitalCity()->AI_bestUnitAI(UNITAI_WORKER, iDummyValue, true, true);
	}

	int iValue = 0;
	if (eBestWorker != NO_UNIT)
	{
		iValue = (GC.getUnitInfo(eBestWorker).getProductionCost() > 0) ? GC.getUnitInfo(eBestWorker).getProductionCost() : 500;

		const int iBestUnitAIValue = AI_unitValue(eBestWorker, UNITAI_WORKER, getCapitalCity()->area());
		const int iThisUnitAIValue = AI_unitValue(pUnit->getUnitType(), UNITAI_WORKER, getCapitalCity()->area());

		//	Value as cost of production of the unit we can build scaled by their relative AI value
		iValue = (iThisUnitAIValue * iValue) / std::max(1, iBestUnitAIValue);
	}
	else
	{
		iValue = (GC.getUnitInfo(pUnit->getUnitType()).getProductionCost() > 0) ? GC.getUnitInfo(pUnit->getUnitType()).getProductionCost() : 500;
		iValue *= 3;	//	We can't build workers at all so value this up
	}

	//	Normalise for game speed
	iValue = iValue * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent() / 100;
	int iNeededWorkers = 0;

	foreach_(CvArea * pLoopArea, GC.getMap().areas())
	{
		if (pLoopArea->getCitiesPerPlayer(getID()) > 0)
		{
			iNeededWorkers += AI_neededWorkers(pLoopArea);
		}
	}
	if (iNeededWorkers > 0)
	{
		//	If we could use a large number of workers that dosn't means the first one
		//	is worth a huge multiple - scale non-linearly up to 3 times base cost
		const int iScalingPercent = 300 - 400 / (1 + iNeededWorkers);
		iValue = 2 * (iValue * iScalingPercent) / 100;	//	Double final result as approx hammer->gold conversion
	}
	else
	{
		//	We don't really know what to do with it, so can sell cheap
		iValue /= 2;
	}
	// add a % adjustment global to the final result so that XML can adjust the outcome without having to directly tweak the formula which seems to consider all logical causes but not so much the end result.
	int iPercentAdjustment = (GC.getWORKER_TRADE_VALUE_PERCENT_ADJUSTMENT() * iValue) / 100;
	iValue += iPercentAdjustment;

	return std::max(0, iValue);
}

CvCity* CvPlayerAI::findBestCoastalCity() const
{
	PROFILE_EXTRA_FUNC();
	CvCity* pBestCity = NULL;
	bool	bFoundConnected = false;

	foreach_(CvCity * pLoopCity, cities())
	{
		if (pLoopCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
		{
			bool bValid = false;
			const bool bConnected = pLoopCity->isConnectedToCapital(getID());

			if (bConnected)
			{
				bValid = true;
			}
			else if (!bFoundConnected)
			{
				bValid = true;
			}

			if (bValid && (pBestCity == NULL || (bConnected && !bFoundConnected) || pLoopCity->getPopulation() > pBestCity->getPopulation()))
			{
				pBestCity = pLoopCity;
			}

			if (bConnected)
			{
				bFoundConnected = true;
			}
		}
	}

	return pBestCity;
}

int CvPlayerAI::strengthOfBestUnitAI(DomainTypes eDomain, UnitAITypes eUnitAIType) const
{
	PROFILE_FUNC();

	CvUnitSelectionCriteria	noGrowthCriteria;

	noGrowthCriteria.m_bIgnoreGrowth = true;
	const UnitTypes eBestUnit = bestBuildableUnitForAIType(eDomain, eUnitAIType, &noGrowthCriteria);

	if (eBestUnit != NO_UNIT)
	{
		return GC.getUnitInfo(eBestUnit).getCombat();
	}
	// We cannot build any! Take the average of any we already have.
	int iTotal = 0;
	int iCount = 0;
	foreach_(const CvUnit * pLoopUnit, units())
	{
		if (eUnitAIType == NO_UNITAI || pLoopUnit->AI_getUnitAIType() == eUnitAIType)
		{
			iCount++;
			iTotal += pLoopUnit->getUnitInfo().getCombat();
		}
	}
	if (iCount > 0)
	{
		iTotal /= iCount;
	}
	return std::max(1, iTotal);
}

UnitTypes CvPlayerAI::bestBuildableUnitForAIType(DomainTypes eDomain, UnitAITypes eUnitAIType, const CvUnitSelectionCriteria* criteria) const
{
	PROFILE_FUNC();

	// What is the best unit we can produce of this unitAI type
	CvCity* pCapitalCity = getCapitalCity();

	// Handle capital-less civs (aka barbs) by just using an arbitrary city
	int iDummy;
	if (pCapitalCity == NULL)
	{
		pCapitalCity = firstCity(&iDummy);
	}
	if (pCapitalCity == NULL)
	{
		return NO_UNIT;
	}
	switch (eDomain)
	{
		case NO_DOMAIN:
		{
			const UnitTypes eBestUnit = pCapitalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);

			if (eBestUnit != NO_UNIT || pCapitalCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
			{
				return eBestUnit;
			}
			// Drop through and check coastal
		}
		case DOMAIN_SEA:
		{
			CvCity* pCoastalCity =
			(
				pCapitalCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())
				?
				pCapitalCity
				:
				findBestCoastalCity()
			);
			if (pCoastalCity != NULL)
			{
				return pCoastalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);
			}
			break;
		}
		case DOMAIN_LAND:
		case DOMAIN_AIR:
		{
			return pCapitalCity->AI_bestUnitAI(eUnitAIType, iDummy, false, true, criteria);
		}
	}
	return NO_UNIT;
}

int CvPlayerAI::AI_militaryUnitTradeVal(const CvUnit* pUnit) const
{
	PROFILE_FUNC();

	int iValue;
	const UnitTypes eUnit = pUnit->getUnitType();
	const UnitAITypes eAIType = GC.getUnitInfo(eUnit).getDefaultUnitAIType();

	if (eAIType == UNITAI_SUBDUED_ANIMAL)
	{
		int iBestValue = 0;
		CvCity* pEvaluationCity = getCapitalCity();

		if (pEvaluationCity == NULL || (pUnit->getDomainType() == DOMAIN_SEA && !pEvaluationCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())))
		{
			pEvaluationCity = findBestCoastalCity();
		}

		if (pEvaluationCity != NULL)
		{
			const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

			//	Subdued animals are rated primarily on what they can construct
			for (int iI = 0; iI < kUnit.getNumBuildings(); iI++)
			{
				const BuildingTypes eBuilding = (BuildingTypes)kUnit.getBuildings(iI);

				if (canConstruct(eBuilding, false, false, true)
				&& AI_getNumBuildingsNeeded(eBuilding, pUnit->getDomainType() == DOMAIN_SEA) > 0)
				{
					foreach_(CvCity * pLoopCity, cities())
					{
						if (pLoopCity->area() == pEvaluationCity->area() && !pLoopCity->hasBuilding(eBuilding))
						{
							const int iValue = pLoopCity->AI_buildingValue(eBuilding);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
							}
						}
					}
				}
			}
			{
				// This section needs more work, the entire function might need work come to think of it.
				int iValue = 0;
				for (int iI = 0; iI < kUnit.getNumHeritage(); iI++)
				{
					if (canAddHeritage((HeritageTypes)kUnit.getHeritage(iI)))
					{
						iValue += 25;
					}
				}
				if (iValue > iBestValue)
				{
					iBestValue = iValue;
				}
			}

			//	Also check their action outcomes (in the capital)
			for (int iI = 0; iI < kUnit.getNumActionOutcomes(); iI++)
			{
				if (kUnit.getActionOutcomeMission(iI) != NO_MISSION)
				{
					const CvOutcomeList* pOutcomeList = kUnit.getActionOutcomeList(iI);
					if (pOutcomeList->isPossibleInPlot(*pUnit, *(pEvaluationCity->plot()), true))
					{
						const int iValue = pOutcomeList->AI_getValueInPlot(*pUnit, *(pEvaluationCity->plot()), true);
						if (iValue > iBestValue)
						{
							iBestValue = iValue;
						}
					}
				}
			}

			for (int iJ = 0; iJ < GC.getNumUnitCombatInfos(); iJ++)
			{
				if (pUnit->isHasUnitCombat((UnitCombatTypes)iJ))
				{
					const CvUnitCombatInfo& kInfo = GC.getUnitCombatInfo((UnitCombatTypes)iJ);
					for (int iI = 0; iI < kInfo.getNumActionOutcomes(); iI++)
					{
						if (kInfo.getActionOutcomeMission(iI) != NO_MISSION)
						{
							const CvOutcomeList* pOutcomeList = kInfo.getActionOutcomeList(iI);
							if (pOutcomeList->isPossibleInPlot(*pUnit, *(pEvaluationCity->plot()), true))
							{
								const int iValue = pOutcomeList->AI_getValueInPlot(*pUnit, *(pEvaluationCity->plot()), true);
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
								}
							}
						}
					}
				}
			}
		}

		iValue = iBestValue;
	}
	else
	{
		const UnitTypes eBestUnit = bestBuildableUnitForAIType(pUnit->getDomainType(), eAIType);
		if (eBestUnit == NO_UNIT)
		{
			iValue = 2 * GC.getUnitInfo(eUnit).getProductionCost();	// We can't build anything like this so double its value
		}
		else
		{
			const int iBestUnitAIValue = AI_unitValue(eBestUnit, GC.getUnitInfo(eUnit).getDefaultUnitAIType(), getCapitalCity()->area());
			const int iThisUnitAIValue = AI_unitValue(eUnit, GC.getUnitInfo(eUnit).getDefaultUnitAIType(), getCapitalCity()->area());

			//	Value as cost of production of the unit we can build scaled by their relative AI value
			iValue = (iThisUnitAIValue * GC.getUnitInfo(eBestUnit).getProductionCost()) / std::max(1, iBestUnitAIValue);
		}

		//	Normalise for game speed, and double as approximate hammer->gold conversion
		iValue = iValue * GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getHammerCostPercent() / 50;
	}

	logging::logMsg("C2C.log", "AI Unit Value For %s is %d\n", GC.getUnitInfo(eUnit).getDescription(), iValue);
	return iValue;
}

int CvPlayerAI::AI_pledgeVoteTradeVal(const VoteTriggeredData* kData, PlayerVoteTypes ePlayerVote, PlayerTypes ePlayer) const
{
	return 1;
}

int CvPlayerAI::AI_corporationTradeVal(CorporationTypes eCorporation) const
{
	PROFILE_FUNC();

	int iValue = 100;

	foreach_(const CvCity * pLoopCity, cities())
	{
		int iTempValue = 2 * AI_corporationValue(eCorporation, pLoopCity);
		if (!pLoopCity->isHasCorporation(eCorporation))
		{
			iTempValue /= 2;
		}

		iValue += iTempValue;
	}

	int iCompetitorCount = 0;

	for (int iI = 0; iI < GC.getNumCorporationInfos(); iI++)
	{
		if (iI != eCorporation)
		{
			if (hasHeadquarters((CorporationTypes)iI))
			{
				if (GC.getGame().isCompetingCorporation(eCorporation, (CorporationTypes)iI))
				{
					iCompetitorCount++;
				}
			}
		}
	}

	return (iValue * 3) / (3 + iCompetitorCount);

}

int CvPlayerAI::AI_secretaryGeneralTradeVal(VoteSourceTypes eVoteSource, PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	typedef bst::array<int, MAX_TEAMS> VotesArray;
	VotesArray aiVotes;
	aiVotes.assign(0);

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		const TeamTypes teamIType = static_cast<TeamTypes>(iI);
		const CvTeamAI& teamI = GET_TEAM(teamIType);

		if (teamI.isAlive() &&
			teamI.isVotingMember(eVoteSource) && !(getTeam() == teamIType))
		{
			int iBestAttitude = -MAX_INT;
			TeamTypes eBestTeam = NO_TEAM;

			for (int iJ = 0; iJ < MAX_TEAMS; iJ++)
			{
				if (iI == iJ)
					continue;

				const TeamTypes teamJType = static_cast<TeamTypes>(iJ);
				if (GET_TEAM(teamJType).isAlive() &&
					GC.getGame().isTeamVoteEligible(teamJType, eVoteSource))
				{
					if (teamI.isVassal(teamJType))
					{
						aiVotes[iJ] += teamI.getVotes(NO_VOTE, eVoteSource);
					}

					const int iAttitude = teamI.AI_getAttitudeVal(teamJType);

					if (iAttitude > iBestAttitude)
					{
						iBestAttitude = iAttitude;
						eBestTeam = teamJType;
					}
				}
			}
			aiVotes[eBestTeam] += teamI.getVotes(NO_VOTE, eVoteSource);
		}
	}

	VotesArray::iterator maxVotesItr = std::max_element(aiVotes.begin(), aiVotes.end());
	int iMostVotes = *maxVotesItr;
	TeamTypes eLikelyWinner = static_cast<TeamTypes>(std::distance(aiVotes.begin(), maxVotesItr));

	bool bKingMaker = false;
	int iOurVotes = 0;
	int iTheirVotes = 0;
	int iValue = 100;

	if (eLikelyWinner != GET_PLAYER(ePlayer).getTeam())
	{
		iOurVotes = GET_TEAM(getTeam()).getVotes(NO_VOTE, eVoteSource);
		iTheirVotes = aiVotes[GET_PLAYER(ePlayer).getTeam()];

		if ((iOurVotes + iTheirVotes) > iMostVotes)
		{
			bKingMaker = true;
		}

		int iOurSharedCivics = 0;
		int iTheirSharedCivics = 0;
		for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
		{
			if (getCivics((CivicOptionTypes)iI) == GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iI))
			{
				iOurSharedCivics++;
			}

			if (getCivics((CivicOptionTypes)iI) == GET_PLAYER(GET_TEAM(eLikelyWinner).getLeaderID()).getCivics((CivicOptionTypes)iI))
			{
				iTheirSharedCivics++;
			}
		}

		iValue *= std::max(1, iOurSharedCivics);
		iValue /= std::max(1, iTheirSharedCivics);
	}
	else
	{
		bKingMaker = false;
	}

	if (bKingMaker)
	{
		const int iExtraVotes = ((iOurVotes + iTheirVotes) - iMostVotes);

		iValue *= iExtraVotes;
	}

	iValue *= 2;
	return iValue;
}

int CvPlayerAI::AI_getCivicAttitudeChange(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	int iAttitude = 0;

	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (getCivics((CivicOptionTypes)iI) != NO_CIVIC)
		{
			const CvCivicInfo& kCivicOption = GC.getCivicInfo(getCivics((CivicOptionTypes)iI));

			for (int iJ = 0; iJ < GC.getNumCivicOptionInfos(); iJ++)
			{
				const int eCivic = GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iJ);

				if (eCivic != NO_CIVIC)
				{
					iAttitude += kCivicOption.getCivicAttitudeChange(eCivic);
				}
			}
		}
	}

	return iAttitude;
}

int CvPlayerAI::AI_getCivicShareAttitude(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	int iAttitude = 0;
	for (int iI = 0; iI < GC.getNumCivicOptionInfos(); iI++)
	{
		if (NO_CIVIC != getCivics((CivicOptionTypes)iI) && getCivics((CivicOptionTypes)iI) == GET_PLAYER(ePlayer).getCivics((CivicOptionTypes)iI))
		{
			iAttitude += GC.getCivicInfo((CivicTypes)getCivics((CivicOptionTypes)iI)).getAttitudeShareMod();
		}
	}
	return iAttitude;
}


TeamTypes CvPlayerAI::AI_bestJoinWarTeam(PlayerTypes eOtherPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& playerOther = GET_PLAYER(eOtherPlayer);
	const TeamTypes eOtherTeam = playerOther.getTeam();
	const CvTeamAI& teamOther = GET_TEAM(eOtherTeam);
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);

	int iBestValue = 0;
	TeamTypes eWarTeam = NO_TEAM;

	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		const TeamTypes eTeamX = GET_PLAYER((PlayerTypes)iI).getTeam();

		if (eTeam != eTeamX && team.isHasMet(eTeamX)
		&& !teamOther.isDefensivePact(eTeamX) && teamOther.canDeclareWar(eTeamX)
		&& // If we are at war, or they have backstabbed a friend, or they are a mutual enemy
			(
				atWar(eTeamX, eTeam)
				||
				AI_getMemoryCount((PlayerTypes)iI, MEMORY_BACKSTAB_FRIEND) > 0
				||
				playerOther.AI_getAttitude((PlayerTypes)iI) < ATTITUDE_CAUTIOUS
				&&
				AI_getAttitude((PlayerTypes)iI) < ATTITUDE_CAUTIOUS
				)
		&&
			(
				playerOther.isHumanPlayer()
				||
				playerOther.AI_getAttitude(getID()) > GC.getLeaderHeadInfo(playerOther.getPersonalityType()).getDeclareWarRefuseAttitudeThreshold()
				&&
				playerOther.AI_getAttitude((PlayerTypes)iI) < ATTITUDE_PLEASED
				)
		)
		{
			int iValue = team.AI_declareWarTradeVal(eTeamX, eOtherTeam);
			// Favor teams we are already at war with
			if (!atWar(eTeamX, eTeam))
			{
				iValue *= 2;
				iValue /= 3;
			}
			if (iBestValue < iValue)
			{
				iBestValue = iValue;
				eWarTeam = eTeamX;
			}
		}
	}
	return eWarTeam;
}

TeamTypes CvPlayerAI::AI_bestMakePeaceTeam(PlayerTypes ePlayer) const
{
	PROFILE_EXTRA_FUNC();
	//kWarTeam is the team that is at war with the player we want them to make peace with
	const CvTeamAI& kWarTeam = GET_TEAM(GET_PLAYER(ePlayer).getTeam());
	const int iTheirPower = kWarTeam.getPower(true);
	int iBestValue = 250; //set a small threshold
	TeamTypes eBestTeam = NO_TEAM;
	for (int iI = 0; iI < MAX_PC_PLAYERS; iI++)
	{
		if ((iI != ePlayer) && (iI != getID()))
		{
			//kPlayer is who we want kWarTeam to make peace with...
			const CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iI);
			if (kPlayer.isAlive() && !kPlayer.isMinorCiv())
			{
				if (GET_TEAM(getTeam()).isHasMet(kPlayer.getTeam()))
				{
					if (atWar(kPlayer.getTeam(), GET_PLAYER(ePlayer).getTeam()) && !atWar(getTeam(), kPlayer.getTeam()))
					{
						if (GET_PLAYER(ePlayer).AI_isWillingToTalk((PlayerTypes)iI) && kWarTeam.AI_makePeaceTrade(kPlayer.getTeam(), getTeam()) == NO_DENIAL)
						{
							int iValue = std::max(0, AI_getAttitudeVal((PlayerTypes)iI) * 100);
							if (GET_TEAM(getTeam()).AI_getWarPlan(kPlayer.getTeam()) != NO_WARPLAN)
							{
								//We are planning war, peace is not "that" valuable after all
								iValue /= 10;
							}
							if (iTheirPower > GET_TEAM(GET_PLAYER((PlayerTypes)iI).getTeam()).getPower(true) * 3)
							{
								//They are likely to crush the enemy player, we don't want them to win
								iValue *= 4;
							}
							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestTeam = kPlayer.getTeam();
							}
						}
					}
				}
			}
		}
	}
	return eBestTeam;
}

TeamTypes CvPlayerAI::AI_bestStopTradeTeam(PlayerTypes eOtherPlayer) const
{
	PROFILE_EXTRA_FUNC();
	const CvPlayer& playerOther = GET_PLAYER(eOtherPlayer);
	const TeamTypes eOtherTeam = playerOther.getTeam();
	const CvTeamAI& teamOther = GET_TEAM(eOtherTeam);
	const TeamTypes eTeam = getTeam();
	const CvTeamAI& team = GET_TEAM(eTeam);
	const TeamTypes eWorstEnemy = team.AI_getWorstEnemy();

	int iBestValue = MAX_INT;
	TeamTypes eBestTeam = NO_TEAM;

	for (int iI = 0; iI < MAX_PC_TEAMS; iI++)
	{
		const TeamTypes eTeamX = (TeamTypes)iI;

		if (GET_TEAM(eTeamX).isAlive() && eTeam != eTeamX && eOtherTeam != eTeamX
		&& team.isHasMet(eTeamX) && teamOther.isHasMet(eTeamX) && !atWar(eOtherTeam, eTeamX)
		&&
			(
				eWorstEnemy == eTeamX
				|| atWar(eTeamX, eTeam)
				|| team.AI_getAttitude(eTeamX) < ATTITUDE_CAUTIOUS
				|| AI_getMemoryCount(eOtherPlayer, MEMORY_BACKSTAB_FRIEND) > 0
				)
		&& playerOther.canStopTradingWithTeam(eTeamX)
		&& playerOther.AI_stopTradingTrade(eTeamX, getID()) == NO_DENIAL)
		{
			const int iValue = AI_stopTradingTradeVal(eTeamX, eOtherPlayer);
			if (iValue < iBestValue)
			{
				iBestValue = iValue;
				eBestTeam = eTeamX;
			}
		}
	}
	return eBestTeam;
}

int CvPlayerAI::AI_militaryBonusVal(BonusTypes eBonus)
{
	PROFILE_EXTRA_FUNC();
	int iValue = 0;

	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
	{
		if (canTrain((UnitTypes)iI))
		{
			if (GC.getUnitInfo((UnitTypes)iI).getPrereqAndBonus() == eBonus)
			{
				iValue += 1000;
			}

			int iHasOrBonusCount = 0;
			bool bFound = false;

			foreach_(const BonusTypes ePrereqBonus, GC.getUnitInfo((UnitTypes)iI).getPrereqOrBonuses())
			{
				if (ePrereqBonus == eBonus)
				{
					bFound = true;
				}
				else if (hasBonus(ePrereqBonus))
				{
					iHasOrBonusCount++;
				}
			}
			if (bFound)
			{
				iValue += 300;
				iValue /= iHasOrBonusCount;
			}
		}
	}
	return iValue;
}


//Slightly altered form of CvUnitAI::AI_promotionValue()
int CvPlayerAI::AI_promotionValue(PromotionTypes ePromotion, UnitTypes eUnit, const CvUnit* pUnit, UnitAITypes eUnitAI, bool bForBuildUp) const
{
	PROFILE_EXTRA_FUNC();
	int iTemp = 0;
	int iExtra;
	int iValue = 0;
	const CvPromotionInfo& kPromotion = GC.getPromotionInfo(ePromotion);
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);
	const CvPlot* pPlot = pUnit ? pUnit->plot() : NULL;
	const int iMoves = pUnit ? pUnit->maxMoves() : kUnit.getMoves();
	const bool bNoDefensiveBonus = !pUnit && kUnit.isNoDefensiveBonus() || pUnit && pUnit->noDefensiveBonus();

	if (eUnitAI == NO_UNITAI)
	{
		eUnitAI = kUnit.getDefaultUnitAIType();
	}

	if (pUnit)
	{
		for (int iI = 0; iI < kPromotion.getNumAIWeightbyUnitCombatTypes(); iI++)
		{
			if (kPromotion.getAIWeightbyUnitCombatType(iI).iModifier != 0)
			{
				if ((pUnit->isHasUnitCombat(kPromotion.getAIWeightbyUnitCombatType(iI).eUnitCombat)) || (kUnit.hasUnitCombat(kPromotion.getAIWeightbyUnitCombatType(iI).eUnitCombat)))
				{
					iValue += kPromotion.getAIWeightbyUnitCombatType(iI).iModifier;
				}
			}
		}
	}

	if (kUnit.isSpy())
	{
		//Readjust promotion choices favoring security, deception, logistics, escape, improvise,
		//filling in other promotions very lightly because the AI does not yet have situational awareness
		//when using spy promotions at the moment of mission execution.

		//Logistics
		//I & III
		iValue += (kPromotion.getMovesChange() * 20);
		//II
		if (kPromotion.isEnemyRoute()) iValue += 20;
		iValue += (kPromotion.getMoveDiscountChange() * 10);
		//total 20, 30, 20 points

		//Deception
		if (kPromotion.getEvasionChange())
		{
			//Lean towards more deception if deception is already present
			iValue += ((kPromotion.getEvasionChange() * 2) + (pUnit == NULL ? 0 : pUnit->evasionProbability()));
		}//total 20, 30, 40 points

		//Security
		iValue += (kPromotion.getVisibilityChange() * 10);
		//Lean towards more security if security is already present
		iValue += (kPromotion.getInterceptChange() + (pUnit == NULL ? kUnit.getInterceptionProbability() : pUnit->currInterceptionProbability()));
		//total 20, 30, 40 points

		//Escape
		if (kPromotion.getWithdrawalChange())
		{
			iValue += 30;
		}

		//Improvise
		if (kPromotion.getUpgradeDiscount())
		{
			iValue += 20;
		}

		//Loyalty
		if (kPromotion.isAlwaysHeal())
		{
			iValue += 15;
		}

		//Instigator
		//I & II
		if (kPromotion.getEnemyHealChange())
		{
			iValue += 15;
		}
		//III
		if (kPromotion.getNeutralHealChange())
		{
			iValue += 15;
		}

		//Alchemist
		if (kPromotion.getFriendlyHealChange())
		{
			iValue += 15;
		}

		for (int iI = 0; iI < kPromotion.getNumSubCombatChangeTypes(); iI++)
		{
			if (pUnit)
			{
				if (!pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI)))
				{
					iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI), eUnit, pUnit, eUnitAI);
				}
			}
			else if (!kUnit.hasUnitCombat((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI)))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI), eUnit, pUnit, eUnitAI);
			}
		}

		for (int iI = 0; iI < kPromotion.getNumRemovesUnitCombatTypes(); iI++)
		{
			if (pUnit)
			{
				if (pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI)))
				{
					iValue -= AI_unitCombatValue((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI), eUnit, pUnit, eUnitAI);
				}
			}
			else if (kUnit.hasUnitCombat((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI)))
			{
				iValue -= AI_unitCombatValue((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI), eUnit, pUnit, eUnitAI);
			}
		}

		if (pUnit && iValue > 0 && !bForBuildUp)
		{
			//GC.getGame().logOOSSpecial(1,iValue, pUnit->getID());
			iValue += GC.getGame().getSorenRandNum(25, "AI Spy Promote");
		}

		return iValue;
	}

	if (kPromotion.isLeader())
	{
		// Don't consume the leader as a regular promotion
		return 0;
	}

	//	Koshling - this is a horrible kludge really to get the AI to realize that promotions
	//	with a subdue animal bonus are VERY good on hunters.  However, it's VERY hard to figure
	//	this out from first principals because its very indirect (outcome has per-promotion value,
	//	but the outcome itself just defines a unit, which then has a build, which than has value!)
	//	As a result I am just giving a bonus to outcome modifiers for hunters generically, which
	//	CURRENTLY works ok since the available outcomes are basically the animal subdues
	if (eUnitAI == UNITAI_HUNTER ||
		(eUnitAI == UNITAI_HUNTER_ESCORT) ||
		(eUnitAI == UNITAI_GREAT_HUNTER))
	{
		for (int iI = 0; iI < GC.getNumOutcomeInfos(); iI++)
		{
			for (int iJ = 0; iJ < GC.getOutcomeInfo((OutcomeTypes)iI).getNumExtraChancePromotions(); iJ++)
			{
				if (GC.getOutcomeInfo((OutcomeTypes)iI).getExtraChancePromotion(iJ) == ePromotion)
				{
					iValue += 2 * GC.getOutcomeInfo((OutcomeTypes)iI).getExtraChancePromotionChance(iJ);
					break;
				}
			}
		}
	}

	if (kPromotion.isBlitz() && !bForBuildUp)
	{
		//ls612: AI to know that Blitz is only useful on units with more than one move now that the filter is gone
		if (iMoves > 1)
		{
			if ((eUnitAI == UNITAI_RESERVE ||
				eUnitAI == UNITAI_ATTACK ||
				eUnitAI == UNITAI_ATTACK_CITY ||
				eUnitAI == UNITAI_PARADROP))
			{
				iValue += (10 * iMoves);
			}
			else
			{
				iValue += 2;
			}
		}
		else
		{
			iValue += 0;
		}
	}

	if (kPromotion.isOneUp())
	{
		if (eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ESCORT)
		{
			iValue += 20;
		}
		else iValue += 5;
	}


	if (kPromotion.isDefensiveVictoryMove())
	{
		if (eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_ATTACK)
		{
			iValue += 12;
		}
		else
		{
			iValue += 8;
		}
	}


	if (kPromotion.isFreeDrop())
	{
		if (eUnitAI == UNITAI_PILLAGE || eUnitAI == UNITAI_ATTACK)
		{
			iValue += 10;
		}
		else iValue += 8;
	}


	if (kPromotion.isOffensiveVictoryMove())
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}

		if (pUnit)
		{
			if (pUnit->isBlitz() || pUnit->isFreeDrop())
			{
				iTemp *= 20;
			}
		}
	}
	iValue += iTemp;

	iTemp = kPromotion.getExcileChange();
	if (iTemp < 0)
	{
		if (pUnit)
		{
			if (pUnit->isExcile())
			{
				iValue += 25;
			}
		}
	}
	if (iTemp > 0)
	{
		if (pUnit)
		{
			if (!pUnit->isExcile())
			{
				iValue -= 25;
			}
		}
	}

	iTemp = kPromotion.getPassageChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			if (!pUnit->isPassage())
			{
				if (eUnitAI == UNITAI_ESCORT)
				{
					iValue += 50;
				}
				else
				{
					iValue += 25;
				}
			}
		}
	}
	if (iTemp < 0)
	{
		if (pUnit)
		{
			if (pUnit->isPassage())
			{
				if (eUnitAI == UNITAI_ESCORT)
				{
					iValue -= 100;
				}
				else
				{
					iValue -= 25;
				}
			}
		}
	}

	//iTemp = kPromotion.getNoNonOwnedCityEntryChange();
	//iTemp = kPromotion.getBlendIntoCityChange();
	//Perhaps these will be interesting for differing AI types so it's here as a reminder
	//No automatic value to be assigned at the moment.

	iTemp = kPromotion.getBarbCoExistChange();
	if (iTemp > 0)
	{
		if (eUnitAI == UNITAI_EXPLORE)
		{
			iTemp += 20;
		}
		else if (eUnitAI == UNITAI_ESCORT)
		{
			iTemp -= 100;
		}
	}
	if (iTemp < 0)
	{
		if (pUnit)
		{
			if (pUnit->isBarbCoExist())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iValue += 15;
				}
				else if (eUnitAI == UNITAI_ESCORT)
				{
					iTemp += 100;
				}
			}
		}
	}

	{
		iTemp = 0;
		if (kPromotion.isPillageEspionage())
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		iValue += iTemp;

		if (kPromotion.isPillageMarauder())
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		iValue += iTemp;

		if (kPromotion.isPillageOnMove())
		{
			if (pUnit)
			{
				if (pUnit->isPillageEspionage()
				||  pUnit->isPillageMarauder()
				||  pUnit->isPillageResearch())
				{
					if (eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 16;
					}
					else iTemp += 8;
				}
				else iTemp++;
			}
			else iTemp++;
		}
		iValue += iTemp;

		if (kPromotion.isPillageOnVictory())
		{
			if (pUnit)
			{
				if (pUnit->isPillageEspionage()
				|| pUnit->isPillageMarauder()
				|| pUnit->isPillageResearch())
				{
					if (eUnitAI == UNITAI_PILLAGE
					|| eUnitAI == UNITAI_ATTACK
					|| eUnitAI == UNITAI_PARADROP
					|| eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 20;
					}
					else iTemp += 12;
				}
				else iTemp += 4;
			}
			else iTemp += 4;
		}
		iValue += iTemp;

		if (kPromotion.isPillageResearch())
		{
			if (pUnit)
			{
				if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
				{
					if (eUnitAI == UNITAI_PILLAGE)
					{
						iTemp += 12;
					}
					else if (
						eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PARADROP
					||  eUnitAI == UNITAI_INFILTRATOR)
					{
						iTemp += 8;
					}
					else
					{
						iTemp += 4;
					}
				}
				else if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 8;
				}
				else if (
					eUnitAI == UNITAI_ATTACK
				||  eUnitAI == UNITAI_PARADROP
				||  eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 5;
				}
				else iTemp += 2;
			}
			else iTemp += 2;
		}
		if (iMoves > 1)
		{
			iTemp *= 100;
		}
		iValue += iTemp;
	}

	iTemp = kPromotion.getAirCombatLimitChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iValue += std::min(iTemp, 20);
		}
	}

	iTemp = kPromotion.getCelebrityHappy();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_HEALER
		||  eUnitAI == UNITAI_HEALER_SEA
		||  eUnitAI == UNITAI_PROPERTY_CONTROL
		||  eUnitAI == UNITAI_PROPERTY_CONTROL_SEA)
		{
			iValue += iTemp * 10;

			if (pUnit)
			{
				const CvCity* pCity = pPlot->getPlotCity();
				if (pCity && pCity->happyLevel() < pCity->unhappyLevel())
				{
					iValue += iTemp * 40;
				}
			}
		}
	}

	iTemp = kPromotion.getCollateralDamageLimitChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getCollateralDamageMaxUnitsChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (15 * iTemp);
		}
	}

	iTemp = kPromotion.getCombatLimitChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				iTemp *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iTemp /= 4;
				}
				iValue += iTemp;
			}
			else
			{
				int iCombatLimitValueProcessed = std::min(iTemp, pUnit->combatLimit() - 100) * 3;
				if (eUnitAI != UNITAI_COLLATERAL)
				{
					iCombatLimitValueProcessed /= 4;
				}

				iValue += iCombatLimitValueProcessed;
			}
		}
		else if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 3);
		}
	}

	iTemp = kPromotion.getExtraDropRange();
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 20 * iTemp;
		}
	}


	iTemp = kPromotion.getSurvivorChance();
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 5 * iTemp;
		}
		else
		{
			iValue += 2 * iTemp;
		}
	}

	iTemp = 0;
	int iWeight = 0;
	const CvPropertyManipulators* promotionPropertyManipulators = GC.getPromotionInfo(ePromotion).getPropertyManipulators();

	if (promotionPropertyManipulators)
	{
		foreach_(const CvPropertySource * pSource, promotionPropertyManipulators->getSources())
		{
			if (pSource->getType() == PROPERTYSOURCE_CONSTANT)
			{
				const PropertyTypes eProperty = pSource->getProperty();
				int iTemp2 = ((CvPropertySourceConstant*)pSource)->getAmountPerTurn(getGameObject());
				if (pUnit && iTemp2 != 0)
				{
					foreach_(const CvPropertySource * uSource, GC.getUnitInfo(pUnit->getUnitType()).getPropertyManipulators()->getSources())
					{
						if (uSource->getType() == PROPERTYSOURCE_CONSTANT && eProperty == uSource->getProperty())
						{
							iTemp2 += ((CvPropertySourceConstant*)uSource)->getAmountPerTurn(getGameObject());
						}
					}
				}
				iTemp += iTemp2 * GC.getPropertyInfo(eProperty).getAIWeight();
			}
		}

		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_PROPERTY_CONTROL
			||  eUnitAI == UNITAI_PROPERTY_CONTROL_SEA)
			{
				iValue += 10 * iTemp;

				if (pUnit)
				{
					const MissionAITypes eMissionAI = pUnit->getGroup()->AI_getMissionAIType();
					if (eMissionAI == MISSIONAI_PROPERTY_CONTROL_RESPONSE
					||  eMissionAI == MISSIONAI_PROPERTY_CONTROL_MAINTAIN)
					{
						iValue += 10 * iTemp;
					}
				}
			}
			else if (
			    eUnitAI == UNITAI_HEALER
			||  eUnitAI == UNITAI_HEALER_SEA
			||  eUnitAI == UNITAI_CITY_DEFENSE
			||  eUnitAI == UNITAI_CITY_SPECIAL
			||  eUnitAI == UNITAI_INVESTIGATOR
			||  eUnitAI == UNITAI_ESCORT
			||  eUnitAI == UNITAI_SEE_INVISIBLE
			||  eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
			{
				iValue += iTemp / 10;
			}
			else if (
			    eUnitAI == UNITAI_BARB_CRIMINAL
			||  eUnitAI == UNITAI_INFILTRATOR)
			{
				iValue -= 10 * iTemp;
			}
		}
	}

	iTemp = kPromotion.getHiddenNationalityChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!pUnit->isHiddenNationality() && iTemp > 0)
			{
				if ((eUnitAI == UNITAI_INFILTRATOR))
				{
					iValue += 100;
				}
				else if (eUnitAI == UNITAI_ESCORT)
				{
					iValue += 50;
				}
				iValue += 30;
			}
			else if (pUnit->isHiddenNationality() && iTemp < 0)
			{
				if (eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue -= 100;
				}

				iValue -= 30;
			}
		}
	}

	//TBHEAL review
	//isNoSelfHeal()
	if (kPromotion.isNoSelfHeal())
	{
		if (pUnit)
		{
			if (!pUnit->hasNoSelfHeal())
			{
				iValue -= 50;
			}
		}
		else
		{
			iValue -= 25;
		}
	}

	iTemp = kPromotion.getMaxHPChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getExtraMaxHP();
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY_LEMMING) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 15;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 10;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kPromotion.getSelfHealModifier();
	if (iTemp > 0)
	{
		if (bForBuildUp)
		{
			if (pUnit)
			{
				iTemp *= pUnit->getDamage();
			}
			iValue += iTemp;
		}
		else
		{
			if (pUnit)
			{
				iTemp += pUnit->getSelfHealModifierTotal();
				if (pUnit->hasNoSelfHeal())
				{
					iTemp *= 2;
				}
			}

			if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_RESERVE) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
			{
				iTemp *= 10;
			}
			else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
					(eUnitAI == UNITAI_COLLATERAL) ||
					(eUnitAI == UNITAI_PILLAGE) ||
					(eUnitAI == UNITAI_CITY_DEFENSE) ||
					(eUnitAI == UNITAI_CITY_COUNTER) ||
					(eUnitAI == UNITAI_ATTACK_SEA) ||
					(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
					(eUnitAI == UNITAI_PIRATE_SEA) ||
					(eUnitAI == UNITAI_ATTACK_AIR) ||
					(eUnitAI == UNITAI_PARADROP) ||
					(eUnitAI == UNITAI_PILLAGE_COUNTER))
			{
				iTemp *= 5;
			}
			else
			{
				iTemp *= 2;
			}

			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getEnemyHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iTemp *= 4;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kPromotion.getNeutralHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 2;
		}

		iValue += iTemp;
	}

	iTemp = kPromotion.getFriendlyHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	iTemp = kPromotion.getVictoryHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp = 0;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 5;
		}

		iValue += iTemp;
	}

	iTemp = kPromotion.getNumHealSupport();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 20;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 5;
		}
		if (bForBuildUp && pPlot != NULL && pUnit != NULL)
		{
			UnitCombatTypes eUnitCombat = pUnit->getBestHealingTypeConst();
			if (eUnitCombat != NO_UNITCOMBAT)
			{
				DomainTypes eDomain = pUnit->getDomainType();
				int iUnsup = pPlot->getInjuredUnitCombatsUnsupportedByHealer(pUnit->getOwner(), eUnitCombat, eDomain);
				iValue += iTemp * (iUnsup * 10);
			}
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getSameTileHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 6;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 2;
		}

		if (bForBuildUp && pPlot != NULL && pUnit != NULL)
		{
			UnitCombatTypes eUnitCombat = pUnit->getBestHealingTypeConst();
			if (eUnitCombat != NULL)
			{
				DomainTypes eDomain = pUnit->getDomainType();
				int iInjured = pPlot->plotCount(PUF_isInjuredUnitCombatType, eUnitCombat, eDomain, NULL, pUnit->getOwner());
				iValue += iTemp * (iInjured * 5);
			}
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getAdjacentTileHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 5;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	if (kPromotion.getNumHealUnitCombatChangeTypes() > 0)
	{
		for (int iK = 0; iK < kPromotion.getNumHealUnitCombatChangeTypes(); iK++)
		{
			UnitCombatTypes eHealUnitCombat = kPromotion.getHealUnitCombatChangeType(iK).eUnitCombat;
			iTemp = kPromotion.getHealUnitCombatChangeType(iK).iHeal;
			iTemp += kPromotion.getHealUnitCombatChangeType(iK).iAdjacentHeal;
			iTemp += kPromotion.getNumHealSupport();

			if (iTemp > 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
					iTemp += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
					iTemp += (pUnit->getSameTileHeal() * 2);
					iTemp += ((pUnit->getAdjacentTileHeal() * 2) / 3);

					iTemp *= (pUnit->getNumHealSupportTotal() + kPromotion.getNumHealSupport());
				}
				if ((eUnitAI == UNITAI_HEALER) ||
					(eUnitAI == UNITAI_HEALER_SEA))
				{
					iTemp *= 4;
				}
				else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
					(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
					(eUnitAI == UNITAI_ESCORT_SEA) ||
					(eUnitAI == UNITAI_HUNTER_ESCORT) ||
					(eUnitAI == UNITAI_ESCORT))
				{
					iTemp *= 2;
					iTemp /= 3;
				}
				else
				{
					iTemp /= 4;
				}
				if (bForBuildUp && pPlot != NULL && pUnit != NULL)
				{
					DomainTypes eDomain = pUnit->getDomainType();
					int iInjured = pPlot->plotCount(PUF_isInjuredUnitCombatType, eHealUnitCombat, eDomain, NULL, pUnit->getOwner());
					iValue += iTemp * (iInjured * 5);
				}
				else
				{
					iValue += iTemp;
				}
			}
		}
	}


	iTemp = kPromotion.getVictoryStackHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = (pUnit->getSameTileHeal() * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).isHealsAs())
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + kPromotion.getNumHealSupport());
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	iTemp = kPromotion.getVictoryAdjacentHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = (pUnit->getAdjacentTileHeal() * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).isHealsAs())
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + kPromotion.getNumHealSupport());
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	if (kPromotion.isAlwaysHeal())
	{
		if ((eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 35;
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 15;
		}
		else
		{
			iValue += 8;
		}
	}
	//

	if (kPromotion.isAmphib())
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 25;
		}
		else
		{
			iValue++;
		}
	}

	if (kPromotion.isRiver())
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 15;
		}
		else
		{
			iValue++;
		}
	}

	if (kPromotion.isEnemyRoute())
	{
		if (eUnitAI == UNITAI_PILLAGE)
		{
			iValue += (50 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
				   (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (30 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (20 + (4 * iMoves));
		}
		else
		{
			iValue += (4 * iMoves);
		}
	}

	if (kPromotion.isHillsDoubleMove())
	{
		if (eUnitAI == UNITAI_EXPLORE ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 50;
		}
		else
		{
			iValue += 10;
		}
	}

	if (kPromotion.isCanMovePeaks() && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 50;
	}
	if (kPromotion.isCanLeadThroughPeaks() && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{// Ability to lead a stack through mountains
		iValue += 75;
	}


	if (kPromotion.isImmuneToFirstStrikes()
		&& (pUnit == NULL || !pUnit->immuneToFirstStrikes()))
	{
		if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 25;
		}
		else if ((eUnitAI == UNITAI_ATTACK))
		{
			iValue += 15;
		}
		else
		{
			iValue += 10;
		}
	}

	iTemp = kPromotion.getInsidiousnessChange();
	if (iTemp != 0)
	{
		//TB Must update here as soon as I can improve on AI - not sure about pirates yet.
		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp *= 10;
		}
		else if (/*(eUnitAI == UNITAI_PIRATE_SEA) ||*/
			(eUnitAI == UNITAI_PILLAGE))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp *= -1;
		}
		if (pUnit)
		{
			if (pPlot != NULL)
			{
				CvCity* pCity = pPlot->getPlotCity();
				if (pCity != NULL && pCity->getOwner() != pUnit->getOwner())
				{
					iTemp *= 2;
				}
			}
		}
		iValue += iTemp;
	}

	iTemp = kPromotion.getInvestigationChange();
	if (iTemp != 0)
	{
		//TB: Do I need another AI for just this?  hmm...
		//Perhaps work in some temporary situational value at least since buildups will likely apply
		//And only the best at this on the tile will make a difference.
		if (eUnitAI == UNITAI_PROPERTY_CONTROL ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			iTemp *= 2;
			if (pUnit)
			{
				if (pPlot != NULL)
				{
					const CvCity* pCity = pPlot->getPlotCity();
					if (pCity != NULL)
					{
						int iNumCriminals = pPlot->getNumCriminals();
						if (eUnitAI == UNITAI_INVESTIGATOR)
						{
							iNumCriminals += 10;
						}
						iTemp += iTemp * (iNumCriminals * iNumCriminals);
					}
				}
			}
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getAssassinChange();
	if (iTemp != 0)
	{
		int iTemp2 = 0;
		if (pUnit)
		{
			if (pUnit->isAssassin())
			{
				if (iTemp < 0)
				{
					iTemp2 -= 30;
				}
				else if (iTemp > 0)
				{
					iTemp2 += 10;
				}
			}
			else if (iTemp < 0)
			{
				iTemp2 -= 5;
			}
			else
			{
				iTemp2 += 30;
			}
		}
		else iTemp2 += iTemp * 5;

		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp2 *= 2;
		}
		iValue += iTemp2;
	}

	iTemp = kPromotion.getVisibilityChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_SEE_INVISIBLE) ||
			(eUnitAI == UNITAI_SEE_INVISIBLE_SEA))
		{
			iValue += (iTemp * 50);
		}
		else if ((eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iValue += (iTemp * 40);
		}
		else if (eUnitAI == UNITAI_PIRATE_SEA)
		{
			iValue += (iTemp * 20);
		}
	}

	iTemp = kPromotion.getMovesChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ASSAULT_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_HEALER) ||
				(eUnitAI == UNITAI_HEALER_SEA) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 40);
		}
		else
		{
			iValue += (iTemp * 25);
		}
	}

	iTemp = kPromotion.getMoveDiscountChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HEALER_SEA) ||
			(eUnitAI == UNITAI_ESCORT) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 20);
		}
		else
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kPromotion.getAirRangeChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR ||
			eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 20);
		}
		else if (eUnitAI == UNITAI_DEFENSE_AIR)
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kPromotion.getInterceptChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_DEFENSE_AIR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_ESCORT_SEA)
		{
			iValue += (iTemp * 4);
		}
		else if (eUnitAI == UNITAI_CITY_SPECIAL || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kPromotion.getEvasionChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kPromotion.getFirstStrikesChange() * 2;
	iTemp += kPromotion.getChanceFirstStrikesChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 25;
			iExtra = pUnit == NULL ? kUnit.getChanceFirstStrikes() + kUnit.getFirstStrikes() * 2 : pUnit->getExtraChanceFirstStrikes() + pUnit->getExtraFirstStrikes() * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp * 5);
		}
	}


	iTemp = kPromotion.getStealthStrikesChange() * 2;
	int iInvisFactor = 0;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				for (int i = 0; i < GC.getNumInvisibleInfos(); i++)
				{
					iInvisFactor += pUnit->invisibilityIntensityTotal((InvisibleTypes)i);
				}
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp *= 25;
			iExtra = pUnit == NULL ? kUnit.getStealthStrikes() * 2 : pUnit->getExtraStealthStrikes() * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;

			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	iTemp = kPromotion.getWithdrawalChange();
	if (iTemp != 0)
	{
		iExtra = (kUnit.getWithdrawalProbability() + (pUnit == NULL ? 0 : pUnit->getExtraWithdrawal() * 4));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_EXPLORE)
		{
			iValue += (iTemp * 4) / 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_PILLAGE) ||
			  (eUnitAI == UNITAI_INFILTRATOR) ||
			  (pUnit != NULL && pUnit->getLeaderUnitType() != NO_UNIT))
		{
			iValue += iTemp * 1;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	//TB Combat Mods Begin
	iTemp = kPromotion.getPursuitChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPursuit() : pUnit->getExtraPursuit();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			((GC.getGame().isModderGameOption(MODDERGAMEOPTION_DEFENDER_WITHDRAW) && (eUnitAI == UNITAI_ATTACK_SEA)) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_HUNTER)) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ATTACK))
		{
			iValue += ((iTemp * 4) / 3);
		}
		else if (eUnitAI == UNITAI_INVESTIGATOR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_SEE_INVISIBLE ||
			eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kPromotion.getEarlyWithdrawChange();
	int itempwithdraw = 0;
	if ((kUnit.getWithdrawalProbability() + (pUnit == NULL ? 0 : pUnit->getExtraWithdrawal() * 4)) > 0)
	{
		itempwithdraw += ((kUnit.getWithdrawalProbability() + (pUnit == NULL ? 0 : pUnit->getExtraWithdrawal() * 4)) / 2);
	}
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? 0 : (itempwithdraw + pUnit->earlyWithdrawTotal());
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 2);
		}
		else if (eUnitAI == UNITAI_ESCORT)
		{
			iValue -= (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kPromotion.getUpkeepModifier();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_INVESTIGATOR) ||
			(eUnitAI == UNITAI_INFILTRATOR) ||
			(eUnitAI == UNITAI_SEE_INVISIBLE))
		{
			iTemp *= 2;
		}
		iValue -= iTemp;
	}

	iTemp = kPromotion.getRBombardDamageChange();
	if (iTemp != 0 && pUnit != NULL && pUnit->canRBombard(true))
	{
		iExtra = pUnit->rBombardDamage();
		iTemp *= (100 + iExtra);
		iTemp /= 400;
		iValue += iTemp;
	}

	iTemp = kPromotion.getRBombardDamageLimitChange();
	if (iTemp != 0 && pUnit != NULL && pUnit->canRBombard(true))
	{
		iExtra = pUnit->rBombardDamageLimit();
		iTemp *= (100 + iExtra);
		iTemp /= 400;
		iValue += iTemp;
	}

	iTemp = kPromotion.getRBombardDamageMaxUnitsChange();
	if (iTemp != 0 && pUnit != NULL && pUnit->canRBombard(true))
	{
		iExtra = pUnit->rBombardDamageMaxUnits();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp * 4;
	}

	iTemp = kPromotion.getDCMBombRangeChange();
	if (iTemp != 0 && pUnit != NULL && pUnit->canRBombard(true))
	{
		iExtra = pUnit->getDCMBombRange();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp * 5;
	}

	iTemp = kPromotion.getDCMBombAccuracyChange();
	if (iTemp != 0 && pUnit != NULL && pUnit->canRBombard(true))
	{
		iExtra = pUnit->getDCMBombAccuracy();
		iTemp *= (100 + iExtra);
		iTemp /= 400;
		iValue += iTemp;
	}
	//TB Notes: assume that our units value Armor and Puncture in a rather generic accross the board value
	//(at least for now.  Attackers should probably favor puncture while defenders would favor armor
	//as armor should come with mobility dampeners (though would likely be included in the eval anyhow...)
	iTemp = kPromotion.getArmorChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getArmor() : pUnit->armorTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kPromotion.getPunctureChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPuncture() : pUnit->punctureTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kPromotion.getDamageModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDamageModifier() : pUnit->damageModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kPromotion.getDodgeModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDodgeModifier() : pUnit->dodgeTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kPromotion.getPrecisionModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPrecisionModifier() : pUnit->precisionTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kPromotion.getCriticalModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCriticalModifier() : pUnit->criticalModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	{
		const int iNumPowerShots = kPromotion.getPowerShotsChange() + (pUnit ? pUnit->powerShotsTotal() : kUnit.getPowerShots());

		iTemp = kPromotion.getPowerShotsChange();
		if (iTemp != 0)
		{
			iTemp *= (
				100
				+ (pUnit ? pUnit->powerShotCombatModifierTotal() : kUnit.getPowerShotCombatModifier())
				+ (pUnit ? pUnit->powerShotCriticalModifierTotal() : kUnit.getPowerShotCriticalModifier())
				+ (pUnit ? pUnit->powerShotPunctureModifierTotal() : kUnit.getPowerShotPunctureModifier())
				+ (pUnit ? pUnit->powerShotPrecisionModifierTotal() : kUnit.getPowerShotPrecisionModifier())
				+ kPromotion.getPowerShotCombatModifierChange()
				+ kPromotion.getPowerShotCriticalModifierChange()
				+ kPromotion.getPowerShotPunctureModifierChange()
				+ kPromotion.getPowerShotPrecisionModifierChange()
			);
			if (iNumPowerShots > 0)
			{
				iValue += iTemp / 10;
			}
			else iValue += iTemp / 100;
		}

		if (iNumPowerShots > 0)
		{
			iTemp = kPromotion.getPowerShotCombatModifierChange();
			if (iTemp != 0)
			{
				iTemp *= (100 + 2*(pUnit ? pUnit->powerShotCombatModifierTotal() : kUnit.getPowerShotCombatModifier()));
				iValue += iTemp * iNumPowerShots / 500;
			}

			iTemp = kPromotion.getPowerShotCriticalModifierChange();
			if (iTemp != 0)
			{
				iTemp *= (100 + 2*(pUnit ? pUnit->powerShotCriticalModifierTotal() : kUnit.getPowerShotCriticalModifier()));
				iValue += iTemp * iNumPowerShots / 500;
			}

			iTemp = kPromotion.getPowerShotPrecisionModifierChange();
			if (iTemp != 0)
			{
				iTemp *= (100 + 2*(pUnit ? pUnit->powerShotPrecisionModifierTotal() : kUnit.getPowerShotPrecisionModifier()));
				iValue += iTemp * iNumPowerShots / 500;
			}

			iTemp = kPromotion.getPowerShotPunctureModifierChange();
			if (iTemp != 0)
			{
				iTemp *= (100 + 2*(pUnit ? pUnit->powerShotPunctureModifierTotal() : kUnit.getPowerShotPunctureModifier()));
				iValue += iTemp * iNumPowerShots / 500;
			}
		}
	}

	iTemp = kPromotion.getEnduranceChange();
	if (iTemp != 0)
	{
		iValue += (iTemp + (100 + 2*(pUnit ? pUnit->enduranceTotal() : kUnit.getEndurance()))) / 10;
	}

	iTemp = kPromotion.getOverrunChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
			  (eUnitAI == UNITAI_COLLATERAL))
		{
			iExtra = kUnit.getOverrun() + (pUnit == NULL ? 0 : pUnit->getExtraOverrun() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	if (!bNoDefensiveBonus)
	{
		iTemp = kPromotion.getRepelChange();
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_CITY_DEFENSE
			||	eUnitAI == UNITAI_CITY_SPECIAL
			||	eUnitAI == UNITAI_CITY_COUNTER
			||	eUnitAI == UNITAI_COUNTER
			||	eUnitAI == UNITAI_RESERVE
			||	eUnitAI == UNITAI_ESCORT)
			{
				iExtra = kUnit.getRepel() + (pUnit ? pUnit->getExtraRepel() * 2 : 0);
				iValue += ((iTemp * (100 + iExtra)) / 100);
			}
			else
			{
				iValue += (iTemp / 4);
			}
		}
		iTemp = kPromotion.getFortRepelChange();
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_CITY_DEFENSE
			||	eUnitAI == UNITAI_CITY_SPECIAL
			||	eUnitAI == UNITAI_CITY_COUNTER
			||	eUnitAI == UNITAI_COUNTER
			||	eUnitAI == UNITAI_RESERVE)
			{
				iExtra = kUnit.getFortRepel() + (pUnit ? pUnit->getExtraFortRepel() * 2 : 0);
				iValue += iTemp * (100 + iExtra) / 110;
			}
			else
			{
				iValue += iTemp / 4;
			}
		}
		iTemp = kPromotion.getRepelRetriesChange();
		if (iTemp != 0)
		{
			iExtra = kUnit.getRepel();
			if (pUnit)
			{
				iExtra += pUnit->getExtraRepel() * 4 + pUnit->repelRetriesTotal();
			}
			iTemp *= 100 + 2*iExtra;
			iTemp /= 100;
			if (eUnitAI == UNITAI_CITY_DEFENSE
			||  eUnitAI == UNITAI_CITY_SPECIAL
			||  eUnitAI == UNITAI_CITY_COUNTER
			||  eUnitAI == UNITAI_COUNTER
			||  eUnitAI == UNITAI_RESERVE
			||  eUnitAI == UNITAI_ESCORT)
			{
				iValue += iTemp * 25;
			}
			else
			{
				iValue += iTemp;
			}
		}

		if (!bNoDefensiveBonus)
		{
			iTemp = kPromotion.getCityDefensePercent();
			if (iTemp != 0)
			{
				iTemp *= 100 + 2*(kUnit.getCityDefenseModifier() + (pUnit ? pUnit->getExtraCityDefensePercent() : 0));

				if (pPlot && pPlot->isCity(true))
				{
					iTemp *= 2;
				}

				if (eUnitAI == UNITAI_CITY_DEFENSE
				||  eUnitAI == UNITAI_CITY_SPECIAL
				||  eUnitAI == UNITAI_CITY_COUNTER)
				{
					iValue += iTemp / 30;
				}
				else if (
				    eUnitAI == UNITAI_HEALER
				||  eUnitAI == UNITAI_PROPERTY_CONTROL
				||  eUnitAI == UNITAI_INVESTIGATOR)
				{
					iValue += iTemp / 300;
				}
				else iValue += iTemp / 800;
			}

			iTemp = kPromotion.getHillsDefensePercent();
			if (iTemp != 0)
			{
				iTemp *= 100 + 2*(kUnit.getHillsDefenseModifier() + (pUnit ? pUnit->getExtraHillsDefensePercent() : 0));

				if (pPlot && pPlot->isHills())
				{
					iTemp *= 2;
				}

				if (eUnitAI == UNITAI_CITY_DEFENSE
				||  eUnitAI == UNITAI_CITY_SPECIAL
				||  eUnitAI == UNITAI_CITY_COUNTER)
				{
					if (pPlot && pPlot->isCity(true))
					{
						iValue += iTemp / 100;
					}
					else iValue += iTemp / 300;
				}
				else if (
				    eUnitAI == UNITAI_COUNTER
				||  eUnitAI == UNITAI_ESCORT
				||  eUnitAI == UNITAI_HUNTER_ESCORT)
				{
					iValue += iTemp / 500;
				}
				else iValue += iTemp / 1000;
			}
		}
	}

	iTemp = kPromotion.getUnyieldingChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + kUnit.getUnyielding() + (pUnit ? pUnit->getExtraUnyielding() : 0);

		if (eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_PILLAGE_COUNTER
		||  eUnitAI == UNITAI_HUNTER
		||  eUnitAI == UNITAI_HUNTER_ESCORT
		||  eUnitAI == UNITAI_GREAT_HUNTER
		||  eUnitAI == UNITAI_RESERVE)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}

	iTemp = kPromotion.getKnockbackChange();
	if (iTemp != 0)
	{
		iTemp *= (
			100
			+
			2 * (kUnit.getKnockback() + (pUnit ? pUnit->getExtraKnockback() : 0))
			*
			(
				1 + kPromotion.getKnockbackRetriesChange()
				+ (pUnit ? (pUnit->knockbackRetriesTotal()) : kUnit.getKnockbackRetries())
			)
		);
		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}

	iTemp = kPromotion.getKnockbackRetriesChange();
	if (iTemp != 0)
	{
		const int iKnockbackChance = kPromotion.getKnockbackChange() + kUnit.getKnockback() + (pUnit ? pUnit->getExtraKnockback() : 0);
		if (iKnockbackChance > 0)
		{
			iTemp *= 100 + 2 * iKnockbackChance * (1 + (pUnit ? (pUnit->knockbackRetriesTotal()) : kUnit.getKnockbackRetries()));

			if (eUnitAI == UNITAI_COUNTER
			||  eUnitAI == UNITAI_ATTACK
			||  eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += iTemp * 4/100;
			}
			else iValue += iTemp / 100;
		}
	}

	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		iTemp = kPromotion.getFlankingStrengthbyUnitCombatTypeChange(iI);
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(kUnit.getFlankingStrengthbyUnitCombatType(iI) + (pUnit ? pUnit->getExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)iI) : 0));

			if (eUnitAI == UNITAI_COUNTER || eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += iTemp / 125;
			}
			else iValue += iTemp / 1000;
		}
	}

	iTemp = kPromotion.getUnnerveChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(kUnit.getUnnerve() + (pUnit ? pUnit->getExtraUnnerve() : 0));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 200;
		}
		else iValue += iTemp / 800;
	}

	iTemp = kPromotion.getEncloseChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(kUnit.getEnclose() + (pUnit ? pUnit->getExtraEnclose() : 0));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 20;
		}
		else iValue += iTemp / 100;
	}

	iTemp = kPromotion.getLungeChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(kUnit.getLunge() + (pUnit ? pUnit->getExtraLunge() : 0));

		if (eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_PILLAGE)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}

	iTemp = kPromotion.getDynamicDefenseChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(kUnit.getDynamicDefense() + (pUnit ? pUnit->getExtraDynamicDefense() : 0));

		if (eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_CITY_SPECIAL
		||  eUnitAI == UNITAI_CITY_COUNTER
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_HEALER
		||  eUnitAI == UNITAI_PROPERTY_CONTROL
		||  eUnitAI == UNITAI_RESERVE_SEA
		||  eUnitAI == UNITAI_ESCORT_SEA
		||  eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp / 100;
		}
		else iValue += iTemp / 400;
	}


	if (kPromotion.isStampedeChange())
	{
		iValue -= 25;
	}

	if (kPromotion.isMakesDamageCold())
	{
		iValue += 25;
	}

	if (kPromotion.isAddsColdImmunity())
	{
		iValue += 25;
	}

	if (pUnit)
	{
		if (kPromotion.isRemoveStampede() && pUnit->mayStampede())
		{
			iValue += 25;
		}

		if (kPromotion.isMakesDamageNotCold() && pUnit->dealsColdDamage())
		{
			iValue -= 25;
		}

		if (kPromotion.isRemovesColdImmunity() && pUnit->hasImmunitytoColdDamage())
		{
			iValue -= 25;
		}
	}


#ifdef BATTLEWORN
	iTemp = kPromotion.getStrAdjperRndChange();
	if (iTemp != 0)
	{
		iValue += iTemp * 5;
	}

	iTemp = kPromotion.getStrAdjperAttChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getWithdrawAdjperAttChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kPromotion.getStrAdjperDefChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_ESCORT) ||
			  (eUnitAI == UNITAI_ESCORT_SEA))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}
#endif // BATTLEWORN

#ifdef OUTBREAKS_AND_AFFLICTIONS
	for (int iI = 0; iI < kPromotion.getNumAfflictOnAttackChangeTypes(); ++iI)
	{
		if (kPromotion.getAfflictOnAttackChangeType(iI).eAfflictionLine > 0)
		{
			iTemp = kPromotion.getAfflictOnAttackChangeType(iI).iProbabilityChange;

			int iTempWithdraw = 0;

			if (pUnit == NULL ? 0 : pUnit->withdrawalProbability() > 0)
			{
				iTempWithdraw += pUnit->withdrawalProbability();
			}
			if (pUnit == NULL ? 0 : pUnit->earlyWithdrawTotal() > 0)
			{
				iTempWithdraw += ((iTempWithdraw * pUnit->earlyWithdrawTotal()) / 100);
			}

			iTemp += ((iTemp * iTempWithdraw) / 100);

			int iImmediate = kPromotion.getAfflictOnAttackChangeType(iI).iImmediate;
			iImmediate *= 25;

			iTemp += iImmediate;

			iValue += iTemp;
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_OUTBREAKS_AND_AFFLICTIONS))
	{
		for (int iI = 0; iI < GC.getPromotionInfo(ePromotion).getNumCureAfflictionChangeTypes(); ++iI)
		{
			if (kPromotion.getCureAfflictionChangeType(iI) > 0)
			{
				PromotionLineTypes eAfflictionLine = ((PromotionLineTypes)kPromotion.getCureAfflictionChangeType(iI));
				iTemp = 10;
				int iAfflictionCount = getPlayerWideAfflictionCount(eAfflictionLine);
				int iPlotAfflictedCount = 0;
				if (pUnit)
				{
					CvPlot* pUnitPlot = pPlot;
					iPlotAfflictedCount = pUnitPlot->getNumAfflictedUnits(pUnit->getOwner(), eAfflictionLine);
				}
				iPlotAfflictedCount *= 100;
				iAfflictionCount += ((iAfflictionCount * iPlotAfflictedCount) / 100);
				iTemp *= iAfflictionCount;
				if ((eUnitAI == UNITAI_HEALER) ||
					  (eUnitAI == UNITAI_HEALER_SEA))
				{
					iTemp *= 2;
				}

				iValue += iTemp;
			}
		}

		for (int iI = 0; iI < GC.getPromotionInfo(ePromotion).getNumAfflictionFortitudeChangeModifiers(); ++iI)
		{
			iValue += GC.getPromotionInfo(ePromotion).getAfflictionFortitudeChangeModifier(iI).iModifier;
		}

		iTemp = kPromotion.getFortitudeChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(kUnit.getFortitude() + (pUnit ? pUnit->getExtraFortitude() : 0));
			iTemp /= 100;
			iValue += iTemp * 3/4;
		}

		for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
		{
			iTemp = kPromotion.getAidChange(iI);
			if (iTemp != 0)
			{
				iTemp *= 100 + 2*(kUnit.getAidChange(iI) + (pUnit ? pUnit->extraAidChange((PropertyTypes)iI) : 0));
				iTemp /= 100;
				if (eUnitAI == UNITAI_HEALER
				||  eUnitAI == UNITAI_HEALER_SEA
				||  eUnitAI == UNITAI_PROPERTY_CONTROL
				||  eUnitAI == UNITAI_PROPERTY_CONTROL_SEA)
				{
					iValue += iTemp * 6/4;
				}
				else iValue += iTemp * 3/4;
			}
		}
	}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef STRENGTH_IN_NUMBERS
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_STRENGTH_IN_NUMBERS))
	{
		iTemp = kPromotion.getFrontSupportPercentChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFrontSupportPercent() : kUnit.getFrontSupportPercent());
			iValue += iTemp / 25;
		}

		iTemp = kPromotion.getShortRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(pUnit ? pUnit->getExtraShortRangeSupportPercent() : kUnit.getShortRangeSupportPercent());
			iValue += iTemp / 25;
		}

		iTemp = kPromotion.getMediumRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(pUnit ? pUnit->getExtraMediumRangeSupportPercent() : kUnit.getMediumRangeSupportPercent());
			iValue += iTemp / 25;
		}

		iTemp = kPromotion.getLongRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(pUnit ? pUnit->getExtraLongRangeSupportPercent() : kUnit.getLongRangeSupportPercent());
			iValue += iTemp / 25;
		}

		iTemp = kPromotion.getFlankSupportPercentChange();
		if (iTemp != 0)
		{
			iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFlankSupportPercent() : kUnit.getFlankSupportPercent());
			iValue += iTemp / 25;
		}
}
#endif // STRENGTH_IN_NUMBERS

	iTemp = kPromotion.getRoundStunProbChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(kUnit.getRoundStunProb() + (pUnit ? pUnit->getExtraRoundStunProb() : 0));
		iValue += iTemp / 25;
	}
	//TB Combat Mods End

	iTemp = kPromotion.getCollateralDamageChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->getExtraCollateralDamage() : kUnit.getCollateralDamage());
		iTemp /= 100;

		if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += iTemp * 2;
		}
		else if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += iTemp / 2;
		}
		else iValue += iTemp / 6;
	}

	iTemp = kPromotion.getBombardRateChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	//Breakdown
	iTemp = kPromotion.getBreakdownChanceChange();
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canAttack())
		{
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iTemp += (iTemp * 10);
			}
			else
			{
				iTemp += (iTemp / 8);
			}
			iTemp *= pUnit->breakdownDamageTotal();
		}
	}
	iValue += iTemp;

	iTemp = kPromotion.getBreakdownDamageChange();
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canAttack())
		{
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iTemp += (iTemp * 100);
			}
			else
			{
				iTemp += (iTemp / 8);
			}
			iTemp *= pUnit->breakdownChanceTotal();
			iTemp /= 100;
		}
	}
	iValue += iTemp;

	iTemp = kPromotion.getTauntChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->tauntTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += (iTemp * 25);
			}
			else
			{
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = kPromotion.getStrengthModifier();
	if (iTemp != 0)
	{
		iValue += iTemp * 2;
	}

	iTemp = kPromotion.getQualityChange();
	if (iTemp != 0)
	{
		iValue += iTemp * 200;
	}

	iTemp = kPromotion.getGroupChange();
	if (iTemp != 0)
	{
		iValue += iTemp * 200;
	}

	iTemp = kPromotion.getCommandRange();
	if (iTemp != 0)
	{
		iValue += iTemp * 25;
	}
	//end mod

	//@MOD Commanders: control points promotion AI value
	if (pUnit)
	{
		iTemp = kPromotion.getControlPoints();
		if (iTemp != 0)
		{
			iValue += iTemp * (100 + 25 * pPlot->getNumUnits());
		}
	}
	//end mod

	iTemp = kPromotion.getCombatPercent();
	if (iTemp != 0)
	{
		//@MOD Commanders: combat promotion value
		if (eUnitAI == UNITAI_GENERAL)
		{
			iValue += (iTemp * 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else if (eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iInvisFactor = 0;
	iTemp = kPromotion.getStealthCombatModifierChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				for (int i = 0; i < GC.getNumInvisibleInfos(); i++)
				{
					iInvisFactor += pUnit->invisibilityIntensityTotal((InvisibleTypes)i);
				}
			}
		}
		else
		{
			iInvisFactor = 1;
		}

		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR) ||
				(eUnitAI == UNITAI_PILLAGE))
		{
			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	//TB Combat Mod
	iTemp = kPromotion.getStrengthChange();
	if (iTemp != 0)
	{
		iValue += (iTemp * 200);
	}

	int iRank = 0;
	iTemp = kPromotion.getCombatModifierPerSizeMoreChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->sizeRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getCombatModifierPerSizeLessChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->sizeRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getCombatModifierPerVolumeMoreChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->groupRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kPromotion.getCombatModifierPerVolumeLessChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->groupRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	if (pUnit)
	{
		if (pUnit->isAnimal() && kPromotion.getAnimalIgnoresBordersChange() > 0 && !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
		{
			iValue += 50;
		}
	}

	if (kPromotion.getNoDefensiveBonusChange() > 0)
	{
		if (bNoDefensiveBonus)
		{
			iValue -= 5;
		}
		else iValue -= 50;
	}
	else if (kPromotion.getNoDefensiveBonusChange() < 0)
	{
		if (bNoDefensiveBonus)
		{
			iValue += 50;
		}
		else iValue += 5;
	}

	if (kPromotion.isOnslaughtChange())
	{
		iValue += 75;
	}

	if (kPromotion.isAttackOnlyCitiesAdd())
	{
		if (pUnit)
		{
			if (pUnit->canAttack())
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue -= 10;
				}
				else
				{
					iValue -= 50;
				}
			}
		}
	}

	if (kPromotion.isAttackOnlyCitiesSubtract())
	{
		if (pUnit)
		{
			if (pUnit->canAttack())
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue += 50;
				}
				else
				{
					iValue += 1;
				}
			}
		}
	}

	if (kPromotion.isIgnoreNoEntryLevelAdd())
	{
		if (pUnit)
		{
			if (pUnit->canAttack() && !pUnit->canAttackOnlyCities())
			{
				if (pUnit->canIgnoreNoEntryLevel())
				{
					iValue += 5;
				}
				else
				{
					iValue += 20;
				}
			}
		}
	}

	if (kPromotion.isIgnoreNoEntryLevelSubtract())
	{
		if (pUnit)
		{
			if (pUnit->canAttack() && !pUnit->canAttackOnlyCities())
			{
				if (pUnit->canIgnoreNoEntryLevel())
				{
					iValue -= 20;
				}
				else
				{
					iValue -= 5;
				}
			}
		}
	}

	if (pUnit != NULL && GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL))
	{
		if (kPromotion.isIgnoreZoneofControlAdd())
		{
			if (pUnit->canIgnoreZoneofControl())
			{
				iValue += 5;
			}
			else
			{
				iValue += 25;
			}
		}
		if (kPromotion.isIgnoreZoneofControlSubtract())
		{
			if (pUnit->canIgnoreZoneofControl())
			{
				iValue -= 25;
			}
			else
			{
				iValue -= 5;
			}
		}
	}

	if (kPromotion.isFliesToMoveAdd())
	{
		if (pUnit)
		{
			if (pUnit->canFliesToMove())
			{
				iValue += 5;
			}
			else
			{
				iValue += 100;
			}
		}
	}

	if (kPromotion.isFliesToMoveSubtract())
	{
		if (pUnit)
		{
			if (pUnit->canFliesToMove())
			{
				iValue -= 75;
			}
			else
			{
				iValue -= 5;
			}
		}
	}


	if (kPromotion.getNumWithdrawVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getWithdrawVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->withdrawVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getWithdrawVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->withdrawVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getWithdrawVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;

				if ((eUnitAI == UNITAI_ATTACK) || (eUnitAI == UNITAI_ATTACK_CITY) ||
					(eUnitAI == UNITAI_COLLATERAL) ||
					(eUnitAI == UNITAI_EXPLORE) ||
					(eUnitAI == UNITAI_ATTACK_SEA) ||
					(eUnitAI == UNITAI_EXPLORE_SEA) ||
					(eUnitAI == UNITAI_CARRIER_SEA) ||
					(eUnitAI == UNITAI_PIRATE_SEA) ||
					(eUnitAI == UNITAI_ATTACK_CITY_LEMMING) ||
					(eUnitAI == UNITAI_INFILTRATOR))
				{
					iValue += (iTemp * iCombatWeight) / 100;
				}
				else if ((eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
						   (eUnitAI == UNITAI_HUNTER) ||
						   (eUnitAI == UNITAI_HUNTER_ESCORT) ||
						(eUnitAI == UNITAI_GREAT_HUNTER))
				{
					iValue += (iTemp * iCombatWeight) / 200;
				}
				else
				{
					iValue += (iTemp * iCombatWeight) / 400;
				}
			}
		}
	}

	if (kPromotion.getNumPursuitVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getPursuitVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->pursuitVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getPursuitVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->pursuitVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getPursuitVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;

				if ((eUnitAI == UNITAI_ATTACK) ||
					(eUnitAI == UNITAI_COUNTER) ||
					(eUnitAI == UNITAI_CITY_DEFENSE) ||
					(eUnitAI == UNITAI_ATTACK_SEA) ||
					(eUnitAI == UNITAI_ESCORT_SEA) ||
					(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
					(eUnitAI == UNITAI_PIRATE_SEA) ||
					(eUnitAI == UNITAI_INVESTIGATOR))
				{
					iValue += (iTemp * iCombatWeight) / 100;
				}
				else if (eUnitAI == UNITAI_HUNTER ||
					(eUnitAI == UNITAI_HUNTER_ESCORT) ||
					(eUnitAI == UNITAI_GREAT_HUNTER))
				{
					iValue += (iTemp * iCombatWeight) / 200;
				}
				else
				{
					iValue += (iTemp * iCombatWeight) / 400;
				}
			}
		}
	}

	if (kPromotion.getNumRepelVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getRepelVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->repelVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getRepelVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->repelVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getRepelVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;

				if ((eUnitAI == UNITAI_COUNTER) ||
					(eUnitAI == UNITAI_CITY_DEFENSE) ||
					(eUnitAI == UNITAI_CITY_COUNTER) ||
					(eUnitAI == UNITAI_ESCORT))
				{
					iValue += (iTemp * iCombatWeight) / 100;
				}
				else if ((eUnitAI == UNITAI_CITY_SPECIAL) ||
						   (eUnitAI == UNITAI_ESCORT_SEA))
				{
					iValue += (iTemp * iCombatWeight) / 200;
				}
				else
				{
					iValue += (iTemp * iCombatWeight) / 400;
				}
			}
		}
	}

	if (kPromotion.getNumKnockbackVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getKnockbackVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->knockbackVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getKnockbackVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->knockbackVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getKnockbackVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;

				if ((eUnitAI == UNITAI_ATTACK) ||
					(eUnitAI == UNITAI_ATTACK_CITY) ||
					(eUnitAI == UNITAI_COLLATERAL))
				{
					iValue += (iTemp * iCombatWeight) / 100;
				}
				else
				{
					iValue += (iTemp * iCombatWeight) / 400;
				}
			}
		}
	}

	if (kPromotion.getNumPunctureVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getPunctureVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->punctureVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getPunctureVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->punctureVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getPunctureVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += (iTemp * iCombatWeight) / 100;
			}
		}
	}

	if (kPromotion.getNumArmorVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getArmorVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->armorVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getArmorVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->armorVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getArmorVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += (iTemp * iCombatWeight) / 100;
			}
		}
	}

	if (kPromotion.getNumDodgeVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getDodgeVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->dodgeVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getDodgeVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->dodgeVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getDodgeVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += iTemp * iCombatWeight / 100;
			}
		}
	}

	if (kPromotion.getNumPrecisionVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getPrecisionVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->precisionVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getPrecisionVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->precisionVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getPrecisionVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += (iTemp * iCombatWeight) / 100;
			}
		}
	}

	if (kPromotion.getNumCriticalVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getCriticalVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->criticalVSUnitCombatTotal((UnitCombatTypes)iI) >= 0 || !pUnit && kUnit.getCriticalVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->criticalVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getCriticalVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += (iTemp * iCombatWeight) / 100;
			}
		}
	}

	if (kPromotion.getNumRoundStunVSUnitCombatChangeTypes() > 0)
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			iTemp = kPromotion.getRoundStunVSUnitCombatChangeType(iI);
			if (iTemp != 0)
			{
				int iCombatWeight = 0;
				//Fighting their own kind
				if (!pUnit && kUnit.hasUnitCombat((UnitCombatTypes)iI) || pUnit && pUnit->isHasUnitCombat((UnitCombatTypes)iI))
				{
					if (pUnit && pUnit->roundStunVSUnitCombatTotal((UnitCombatTypes)iI) >=0 || !pUnit && kUnit.getRoundStunVSUnitCombatType(iI) >= 0)
					{
						iCombatWeight = 70;//"axeman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}
				else
				{
					//fighting other kinds
					if (pUnit && pUnit->roundStunVSUnitCombatTotal((UnitCombatTypes)iI) > 10 || !pUnit && kUnit.getRoundStunVSUnitCombatType(iI) > 10)
					{
						iCombatWeight = 70;//"spearman takes formation"
					}
					else
					{
						iCombatWeight = 30;
					}
				}

				iCombatWeight *= AI_getUnitCombatWeight((UnitCombatTypes)iI);
				iCombatWeight /= 100;
				iValue += (iTemp * iCombatWeight) / 100;
			}
		}
	}
	//TB Combat Mods
	//TB Modification note:adjusted City Attack promo value to balance better against withdraw promos for city attack ai units.
	iTemp = kPromotion.getCityAttackPercent();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iTemp *= 100 + kUnit.getCityAttackModifier() + (pUnit ? 2*pUnit->getExtraCityAttackPercent() : 0);
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
			{
				iValue += iTemp * 4;
			}
			else iValue += iTemp;
		}
	}

	iTemp = kPromotion.getHillsAttackPercent();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->getExtraHillsAttackPercent() : kUnit.getHillsAttackModifier());
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_COUNTER)
		{
			iValue += iTemp / 4;
		}
		else iValue += iTemp / 16;
	}


	//Very simple AI for AddsBuildType that assumes they're always desireable.
	for (int iI = 0; iI < kPromotion.getNumAddsBuildTypes(); iI++)
	{
		if ((BuildTypes)kPromotion.getAddsBuildType(iI) != NO_BUILD)
		{
			iValue += 50;
		}
	}

	//WorkRateMod
	iTemp = kPromotion.getHillsWorkPercent();

	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kPromotion.getPeaksWorkPercent();
	//Note: this could use some further development (particularly in ensuring that the unit CAN go onto peaks)
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kPromotion.getCaptureProbabilityModifierChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_INVESTIGATOR || pUnit && pUnit->canAttack())
		{
			iValue += iTemp * 3;
		}
		else if (pUnit && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else
		{
			iValue += 2*(iTemp > 0) - 1;
		}
	}

	iTemp = kPromotion.getCaptureResistanceModifierChange();
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canFight() || eUnitAI == UNITAI_INVESTIGATOR || eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp;
		}
		else iValue += 2*(iTemp > 0) - 1;
	}

	iTemp = kPromotion.getRevoltProtection();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_CITY_DEFENSE
		||	eUnitAI == UNITAI_CITY_COUNTER
		||	eUnitAI == UNITAI_CITY_SPECIAL
		||	eUnitAI == UNITAI_PROPERTY_CONTROL
		||	eUnitAI == UNITAI_INVESTIGATOR)
		{
			if (pUnit != NULL && pPlot != NULL && pUnit->getX() != INVALID_PLOT_COORD && pPlot->isCity())
			{
				PlayerTypes eCultureOwner = pPlot->calculateCulturalOwner();
				// High weight for cities being threatened with culture revolution
				if (eCultureOwner != NO_PLAYER && GET_PLAYER(eCultureOwner).getTeam() != getTeam())
				{
					iValue += iTemp * 15;
				}
			}
		}
	}

	iTemp = kPromotion.getCollateralDamageProtection();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL))
		{
			iValue += (iTemp / 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kPromotion.getPillageChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_PIRATE_SEA ||
			eUnitAI == UNITAI_INFILTRATOR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kPromotion.getUpgradeDiscount();
	if (iTemp != 0)
	{
		iValue += (iTemp / 4);
	}

	iTemp = kPromotion.getExperiencePercent();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA))
		{
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp / 2);
		}
	}

	iTemp = kPromotion.getKamikazePercent();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp / 16);
		}
		else
		{
			iValue += (iTemp / 64);
		}
	}

	int iPass = 0;
	int iTempValue = 0;
	for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
	{
		//Team Project (4)
		//WorkRateMod
		iTemp = kPromotion.getBuildWorkRateModifierChangeType(iI);
		if (iTemp != 0)
		{
			iPass++;
			if (eUnitAI == UNITAI_WORKER)
			{
				int iBuildWeight = 1000;
				//ls612: Weight values by Terrain in area
				if (pUnit)
				{
					//Still more to be done on this.
					//We should be factoring in how many of this build is still needed in the nation
					//only giving consideration to builds currently available to the unit at this time
					//obviously currently needed routes are going to have a lot of weight here if such need exists.
					//@ls612: You've done great work on the ai and I don't want to intrude on this any further than to
					//have simply set up the build rate modifier as a clean example of the unit/promo tag setup chain.
					iBuildWeight = 1000;
				}
				else
				{
					iBuildWeight = 100;
				}
				iTempValue += (iBuildWeight * iTemp) / 1000;
			}
			else
			{
				iTempValue++;
			}
		}
	}
	if (iPass > 0)
	{
		iTempValue /= iPass;
	}
	iValue += iTempValue;

	{
		const bool bTerrainDamage = GC.getGame().isModderGameOption(MODDERGAMEOPTION_TERRAIN_DAMAGE);
		int iPass = 0;
		int iTempValue = 0;
		for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
		{
			if (kPromotion.getTerrainAttackPercent(iI)
			||  kPromotion.getTerrainDefensePercent(iI)
			||  kPromotion.getTerrainWorkPercent(iI)
			||  kPromotion.getTerrainDoubleMove(iI)
			||  bTerrainDamage && kPromotion.getIgnoreTerrainDamage() == iI
			||  kPromotion.getWithdrawOnTerrainTypeChange(iI))
			{
				iPass++;
				const int iTerrainWeight = (
					pUnit
					?
					1000
					*
					pUnit->area()->getNumRevealedTerrainTiles(getTeam(), (TerrainTypes)iI)
					/
					std::max(1, pUnit->area()->getNumRevealedTiles(getTeam()))
					:
					1000
				);
				const bool bOnTerrain = pPlot && pPlot->getTerrainType() == iI;

				iTemp = kPromotion.getTerrainAttackPercent(iI);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraTerrainAttackPercent((TerrainTypes)iI) : kUnit.getTerrainAttackModifier(iI));

					if (bOnTerrain)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					if (eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_COUNTER
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_GREAT_HUNTER)
					{
						iTempValue += iTemp * iTerrainWeight / 1000;
					}
					else
					{
						iTempValue += iTemp * iTerrainWeight / 1400;
					}
				}

				if (!bNoDefensiveBonus)
				{
					iTemp = kPromotion.getTerrainDefensePercent(iI);
					if (iTemp != 0)
					{
						iTemp *= 100 + 2*(pUnit ? pUnit->getExtraTerrainDefensePercent((TerrainTypes)iI) : kUnit.getTerrainDefenseModifier(iI));

						if (bOnTerrain)
						{
							iTemp /= 50;
						}
						else iTemp /= 100;

						if (eUnitAI == UNITAI_COUNTER
						||	eUnitAI == UNITAI_ESCORT
						||	eUnitAI == UNITAI_HUNTER
						||	eUnitAI == UNITAI_HUNTER_ESCORT
						||	eUnitAI == UNITAI_EXPLORE
						||	eUnitAI == UNITAI_GREAT_HUNTER)
						{
							iTempValue += iTemp * iTerrainWeight / 1000;
						}
						else
						{
							iTempValue += iTemp * iTerrainWeight / 1400;
						}
					}
				}

				if (kPromotion.getTerrainDoubleMove(iI))
				{
					iTemp = iMoves * (bOnTerrain ? 80 : 40);

					if (eUnitAI == UNITAI_EXPLORE
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iTerrainWeight / 200;
					}
					else if (
					    eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_COUNTER)
					{
						iTempValue += iTemp * iTerrainWeight / 500;
					}
					else iTempValue += iTemp * iTerrainWeight / 1000;
				}

				iTemp = kPromotion.getTerrainWorkPercent(iI);
				if (iTemp != 0)
				{
					if (eUnitAI == UNITAI_WORKER)
					{
						if (bOnTerrain)
						{
							iTemp *= 2;
						}
						iTempValue += iTemp * iTerrainWeight / 100;
					}
					else iTempValue++;
				}

				if (bTerrainDamage && kPromotion.getIgnoreTerrainDamage() == iI)
				{
					if (pUnit && pUnit->isTerrainProtected((TerrainTypes)iI))
					{
						iTempValue++;
					}
					else
					{
						iTemp = -GC.getTerrainInfo((TerrainTypes)kPromotion.getIgnoreTerrainDamage()).getHealthPercent();

						if (bOnTerrain)
						{
							iTemp *= 2;
						}

						if (eUnitAI == UNITAI_EXPLORE
						||  eUnitAI == UNITAI_HUNTER
						||  eUnitAI == UNITAI_HUNTER_ESCORT
						||  eUnitAI == UNITAI_GREAT_HUNTER
						||  eUnitAI == UNITAI_ESCORT)
						{
							iTempValue += iTemp * iTerrainWeight / 100;
						}
						else if (
						    eUnitAI == UNITAI_ATTACK
						||  eUnitAI == UNITAI_PILLAGE
						||  eUnitAI == UNITAI_COUNTER)
						{
							iTempValue += iTemp * iTerrainWeight / 200;
						}
						else iTempValue += iTemp * iTerrainWeight / 500;
					}
				}

				iTemp = kPromotion.getWithdrawOnTerrainTypeChange(iI);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraWithdrawOnTerrainType((TerrainTypes)iI) : kUnit.getWithdrawOnTerrainType(iI));

					if (bOnTerrain)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					switch (eUnitAI)
					{
						case UNITAI_ANIMAL:
						case UNITAI_COLLATERAL:
						case UNITAI_PILLAGE:
						case UNITAI_EXPLORE:
						case UNITAI_ATTACK_SEA:
						case UNITAI_EXPLORE_SEA:
						case UNITAI_CARRIER_SEA:
						case UNITAI_PIRATE_SEA:
						case UNITAI_SUBDUED_ANIMAL:
						{
							if (pUnit && pUnit->withdrawalProbability() > 0)
							{
								iTempValue += iTemp * iTerrainWeight / 1000;
							}
							else iTempValue += iTemp * iTerrainWeight / 1400;

							break;
						}
						default:
						{
							if (pUnit && pUnit->withdrawalProbability() > 0)
							{
								iTempValue += iTemp * iTerrainWeight / 1800;
							}
							else iTempValue += iTemp * iTerrainWeight / 2200;
						}
					}
				}
			}
		}
		if (iPass > 0)
		{
			iValue += iTempValue / iPass;
		}
	}

	{
		int iPass = 0;
		int iTempValue = 0;
		for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
		{
			if (kPromotion.getFeatureAttackPercent(iI)
			||  kPromotion.getFeatureDefensePercent(iI)
			||  kPromotion.getFeatureWorkPercent(iI)
			||  kPromotion.getFeatureDoubleMove(iI)
			||  kPromotion.getWithdrawOnFeatureTypeChange(iI))
			{
				iPass++;
				const int iFeatureWeight = (
					pUnit
					?
					1000
					*
					pUnit->area()->getNumRevealedFeatureTiles(getTeam(), (FeatureTypes)iI)
					/
					std::max(1, pUnit->area()->getNumRevealedTiles(getTeam()))
					:
					1000
				);
				const bool bOnFeature = pPlot && pPlot->getFeatureType() == iI;

				iTemp = kPromotion.getFeatureAttackPercent(iI);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFeatureAttackPercent((FeatureTypes)iI) : kUnit.getFeatureAttackModifier(iI));

					if (bOnFeature)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					if (eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_COUNTER
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iFeatureWeight / 1000;
					}
					else iTempValue += iTemp * iFeatureWeight / 1400;
				}

				if (!bNoDefensiveBonus)
				{
					iTemp = kPromotion.getFeatureDefensePercent(iI);
					if (iTemp != 0)
					{
						iTemp *= 100 + 2*(pUnit ? pUnit->getExtraFeatureDefensePercent((FeatureTypes)iI) : kUnit.getFeatureDefenseModifier(iI));

						if (bOnFeature)
						{
							iTemp /= 50;
						}
						else iTemp /= 100;

						if (eUnitAI == UNITAI_COUNTER
						||	eUnitAI == UNITAI_EXPLORE
						||	eUnitAI == UNITAI_HUNTER
						||	eUnitAI == UNITAI_HUNTER_ESCORT
						||	eUnitAI == UNITAI_GREAT_HUNTER
						||	eUnitAI == UNITAI_ESCORT)
						{
							iTempValue += iTemp * iFeatureWeight / 1000;
						}
						else iTempValue += iTemp * iFeatureWeight / 1400;
					}
				}

				if (kPromotion.getFeatureDoubleMove(iI))
				{
					iTemp = iMoves * (bOnFeature ? 80 : 40);

					if (eUnitAI == UNITAI_EXPLORE
					||  eUnitAI == UNITAI_HUNTER
					||  eUnitAI == UNITAI_HUNTER_ESCORT
					||  eUnitAI == UNITAI_GREAT_HUNTER
					||  eUnitAI == UNITAI_ESCORT)
					{
						iTempValue += iTemp * iFeatureWeight / 200;
					}
					else if (
					    eUnitAI == UNITAI_ATTACK
					||  eUnitAI == UNITAI_PILLAGE
					||  eUnitAI == UNITAI_COUNTER)
					{
						iTempValue += iTemp * iFeatureWeight / 500;
					}
					else iTempValue += iTemp * iFeatureWeight / 1000;
				}

				iTemp = kPromotion.getWithdrawOnFeatureTypeChange(iI);
				if (iTemp != 0)
				{
					iTemp *= 100 + 2*(pUnit ? pUnit->getExtraWithdrawOnFeatureType((FeatureTypes)iI, true) : kUnit.getWithdrawOnFeatureType(iI));

					if (bOnFeature)
					{
						iTemp /= 50;
					}
					else iTemp /= 100;

					switch (eUnitAI)
					{
						case UNITAI_ANIMAL:
						case UNITAI_COLLATERAL:
						case UNITAI_PILLAGE:
						case UNITAI_EXPLORE:
						case UNITAI_ATTACK_SEA:
						case UNITAI_EXPLORE_SEA:
						case UNITAI_CARRIER_SEA:
						case UNITAI_PIRATE_SEA:
						case UNITAI_SUBDUED_ANIMAL:
						{
							if (pUnit && pUnit->withdrawalProbability() > 0)
							{
								iTempValue += iTemp * iFeatureWeight / 1000;
							}
							else iTempValue += iTemp * iFeatureWeight / 1400;

							break;
						}
						default:
						{
							if (pUnit && pUnit->withdrawalProbability() > 0)
							{
								iTempValue += iTemp * iFeatureWeight / 1800;
							}
							else iTempValue += iTemp * iFeatureWeight / 2200;
						}
					}
				}

				//ls612: Terrain Work Modifiers //TB Edited for WorkRateMod (THANK you for thinking this out ls!)
				iTemp = kPromotion.getFeatureWorkPercent(iI);
				if (iTemp != 0)
				{
					if (eUnitAI == UNITAI_WORKER)
					{
						if (bOnFeature)
						{
							iTemp *= 2;
						}
						iTempValue += iTemp * iFeatureWeight / 100;
					}
					else iTempValue++;
				}
			}
		}
		if (iPass > 0)
		{
			iValue += iTempValue / iPass;
		}
	}


	if ((pUnit && pUnit->canFight() || !pUnit && kUnit.getCombat() > 0))
	{
		for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
		{
			if (pUnit ? pUnit->unitCombatModifier((UnitCombatTypes)iI) >= 0 : kUnit.getUnitCombatModifier(iI) >= 0)
			{
				iValue += kPromotion.getUnitCombatModifierPercent(iI) * 2;
			}
			else
			{
				iValue += kPromotion.getUnitCombatModifierPercent(iI);
			}
		}
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		iTemp = kPromotion.getDomainModifierPercent(iI);
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER)
			{
				iValue += (iTemp * 1);
			}
			else if ((eUnitAI == UNITAI_ATTACK) ||
					   (eUnitAI == UNITAI_RESERVE))
			{
				iValue += (iTemp / 2);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	// TB Combat Mods Begin
	//TB Rudimentary to get us started
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
		{
			iTemp = kPromotion.getVisibilityIntensityChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)iI);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 35);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 15);
				}
				else
				{
					iValue += (iTemp * 10);
				}
			}
			iTemp = kPromotion.getInvisibilityIntensityChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)iI);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 30);
				}
				else
				{
					iValue += (iTemp * 5);
				}
			}
			iTemp = kPromotion.getVisibilityIntensityRangeChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					if (pUnit->visibilityIntensityTotal((InvisibleTypes)iI) != 0)
					{
						iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)iI);
					}
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 35);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 15);
				}
				else
				{
					iValue += (iTemp * 10);
				}
			}
		}
		//VERY rudimentary
		for (int iI = 0; iI < kPromotion.getNumInvisibleTerrainChanges(); iI++)
		{
			iTemp = kPromotion.getInvisibleTerrainChange(iI).iIntensity;
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kPromotion.getInvisibleTerrainChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumInvisibleFeatureChanges(); iI++)
		{
			iTemp = kPromotion.getInvisibleFeatureChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kPromotion.getInvisibleFeatureChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumInvisibleImprovementChanges(); iI++)
		{
			iTemp = kPromotion.getInvisibleImprovementChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kPromotion.getInvisibleImprovementChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 3);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleTerrainChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleTerrainChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleTerrainChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 15);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 8);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleFeatureChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleFeatureChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleFeatureChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 15);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 8);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleImprovementChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleImprovementChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleImprovementChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 15);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 8);
				}
				else
				{
					iValue += (iTemp * 3);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleTerrainRangeChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleTerrainRangeChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleTerrainRangeChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 8);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 4);
				}
				else
				{
					iValue += (iTemp * 2);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleFeatureRangeChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleFeatureRangeChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleFeatureRangeChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 8);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 4);
				}
				else
				{
					iValue += (iTemp * 2);
				}
			}
		}
		for (int iI = 0; iI < kPromotion.getNumVisibleImprovementRangeChanges(); iI++)
		{
			iTemp = kPromotion.getVisibleImprovementChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)kPromotion.getVisibleImprovementRangeChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 8);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 4);
				}
				else
				{
					iValue += (iTemp * 2);
				}
			}
		}
	}

	iTemp = kPromotion.getAttackCombatModifierChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->getExtraAttackCombatModifier() : kUnit.getAttackCombatModifier());
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ANIMAL))
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kPromotion.getDefenseCombatModifierChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->getExtraDefenseCombatModifier() : kUnit.getDefenseCombatModifier());
		iTemp /= 100;

		if (bForBuildUp
		|| eUnitAI == UNITAI_RESERVE
		|| eUnitAI == UNITAI_CITY_DEFENSE
		|| eUnitAI == UNITAI_ESCORT_SEA
		|| eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp * (2 + bForBuildUp);
		}
		else iValue += iTemp;
	}

	if (!isNPC())
	{
		iTemp = kPromotion.getVSBarbsChange();
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER
			||  eUnitAI == UNITAI_CITY_DEFENSE
			||  eUnitAI == UNITAI_CITY_COUNTER
			||  eUnitAI == UNITAI_ESCORT_SEA
			||  eUnitAI == UNITAI_EXPLORE_SEA
			||  eUnitAI == UNITAI_EXPLORE
			||  eUnitAI == UNITAI_PILLAGE_COUNTER
			||  eUnitAI == UNITAI_HUNTER
			||  eUnitAI == UNITAI_HUNTER_ESCORT
			||  eUnitAI == UNITAI_GREAT_HUNTER
			||  eUnitAI == UNITAI_ESCORT)
			{
				iTemp *= 10 - getCurrentEra();
				iValue += iTemp / 9;
			}
		}
	}

	iTemp = kPromotion.getReligiousCombatModifierChange();
	if (iTemp != 0)
	{
		iTemp *= 100 + 2*(pUnit ? pUnit->getExtraReligiousCombatModifier() : kUnit.getReligiousCombatModifier());
		iTemp /= 100;

		if (eUnitAI == UNITAI_ATTACK
		||  eUnitAI == UNITAI_ATTACK_CITY
		||  eUnitAI == UNITAI_COLLATERAL
		||  eUnitAI == UNITAI_PILLAGE
		||  eUnitAI == UNITAI_RESERVE
		||  eUnitAI == UNITAI_COUNTER
		||  eUnitAI == UNITAI_CITY_DEFENSE
		||  eUnitAI == UNITAI_ATTACK_SEA
		||  eUnitAI == UNITAI_RESERVE_SEA
		||  eUnitAI == UNITAI_ASSAULT_SEA
		||  eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iValue += 2 * iTemp;
		}
	}

	// TB Combat Mods Begin
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL) && kPromotion.isZoneOfControl())
	{
		iValue += 250;
	}

	for (int iI = 0; iI < kPromotion.getNumSubCombatChangeTypes(); iI++)
	{
		if (pUnit == NULL)
		{
			if (!kUnit.hasUnitCombat((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI)))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI), eUnit, pUnit, eUnitAI);
			}
		}
		else
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI)))
			{
				iValue += AI_unitCombatValue((UnitCombatTypes)kPromotion.getSubCombatChangeType(iI), eUnit, pUnit, eUnitAI);
			}
		}
	}

	for (int iI = 0; iI < kPromotion.getNumRemovesUnitCombatTypes(); iI++)
	{
		if (pUnit == NULL)
		{
			if (kUnit.hasUnitCombat((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI)))
			{
				iValue -= AI_unitCombatValue((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI), eUnit, pUnit, eUnitAI);
			}
		}
		else
		{
			if (!pUnit->isHasUnitCombat((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI)))
			{
				iValue -= AI_unitCombatValue((UnitCombatTypes)kPromotion.getRemovesUnitCombatType(iI), eUnit, pUnit, eUnitAI);
			}
		}
	}

	if (pUnit != NULL && pUnit->canAcquirePromotion(ePromotion))
	{
		iValue = std::max(1, iValue);
	}

	if (pUnit != NULL && iValue > 0 && !bForBuildUp)
	{
		//GC.getGame().logOOSSpecial(2,iValue, pUnit->getID());
		iValue += GC.getGame().getSorenRandNum(25, "AI Unit Promote");
	}

	return iValue;
}
//TB UNITCOMBAT AI

int CvPlayerAI::AI_unitCombatValue(UnitCombatTypes eUnitCombat, UnitTypes eUnit, const CvUnit* pUnit, UnitAITypes eUnitAI) const
{
	PROFILE_EXTRA_FUNC();
	int iTemp;
	int iExtra;

	int iValue = 0;

	const CvUnitCombatInfo& kUnitCombat = GC.getUnitCombatInfo(eUnitCombat);
	const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

	const int iMoves = pUnit ? pUnit->maxMoves() : kUnit.getMoves();

	if (eUnitAI == NO_UNITAI)
	{
		eUnitAI = kUnit.getDefaultUnitAIType();
	}

	if (kUnit.isSpy())
	{

		//Readjust promotion choices favouring security, deception, logistics, escape, improvise,
		//filling in other promotions very lightly because the AI does not yet have situational awareness
		//when using spy promotions at the moment of mission execution.

			//Logistics
			//I & III
		iValue += (kUnitCombat.getMovesChange() * 20);
		//II
		if (kUnitCombat.isEnemyRoute()) iValue += 20;
		iValue += (kUnitCombat.getMoveDiscountChange() * 10);
		//total 20, 30, 20 points

		//Deception
		if (kUnitCombat.getEvasionChange())
		{
			//Lean towards more deception if deception is already present
			iValue += ((kUnitCombat.getEvasionChange() * 2) + (pUnit == NULL ? 0 : pUnit->evasionProbability()));
		}//total 20, 30, 40 points

		//Security
		iValue += (kUnitCombat.getVisibilityChange() * 10);
		//Lean towards more security if security is already present
		iValue += (kUnitCombat.getInterceptChange() + (pUnit == NULL ? kUnit.getInterceptionProbability() : pUnit->currInterceptionProbability()));
		//total 20, 30, 40 points

		//Escape
		if (kUnitCombat.getWithdrawalChange())
		{
			iValue += 30;
		}

		//Improvise
		if (kUnitCombat.getUpgradeDiscount())
		{
			iValue += 20;
		}

		//Loyalty
		if (kUnitCombat.isAlwaysHeal())
		{
			iValue += 15;
		}

		//Instigator
		//I & II
		if (kUnitCombat.getEnemyHealChange())
		{
			iValue += 15;
		}
		//III
		if (kUnitCombat.getNeutralHealChange())
		{
			iValue += 15;
		}

		//Alchemist
		if (kUnitCombat.getFriendlyHealChange())
		{
			iValue += 15;
		}

		return iValue;
	}

	if (kUnitCombat.isBlitz())
	{
		//ls612: AI to know that Blitz is only useful on units with more than one move now that the filter is gone
		if (iMoves > 1)
		{
			if ((eUnitAI == UNITAI_RESERVE ||
				eUnitAI == UNITAI_ATTACK ||
				eUnitAI == UNITAI_ATTACK_CITY ||
				eUnitAI == UNITAI_PARADROP))
			{
				iValue += (10 * iMoves);
			}
			else
			{
				iValue += 2;
			}
		}
		else
		{
			iValue += 0;
		}
	}

	if (kUnitCombat.isOneUp())
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 20;
		}
		else
		{
			iValue += 5;
		}
	}

	if (kUnitCombat.isDefensiveVictoryMove())
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK))
		{
			iValue += 10;
		}
		else
		{
			iValue += 8;
		}
	}

	if (pUnit && pUnit->noDefensiveBonus())
	{
		iValue *= 100;
	}

	iTemp = 0;
	if (kUnitCombat.isFreeDrop())
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}
	}

	if (kUnitCombat.isOffensiveVictoryMove())
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp += 10;
		}
		else
		{
			iTemp += 8;
		}
	}

	if (pUnit)
	{
		if (pUnit->isBlitz() || pUnit->isFreeDrop())
		{
			iTemp *= 20;
		}
	}
	iValue += iTemp;

	iTemp = 0;
	if (kUnitCombat.isPillageEspionage())
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if ((eUnitAI == UNITAI_ATTACK) ||
						(eUnitAI == UNITAI_PARADROP) ||
						(eUnitAI == UNITAI_INFILTRATOR))
				{
					iTemp += 8;
				}
				else
				{
					iTemp += 4;
				}
			}
			else
			{
				iTemp += 2;
			}
		}
		else
		{
			iTemp += 2;
		}
	}

	if (kUnitCombat.isPillageMarauder())
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_PARADROP || eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 8;
				}
				else iTemp += 4;
			}
			else iTemp += 2;
		}
		else iTemp += 2;
	}

	if (kUnitCombat.isPillageOnMove())
	{
		if (pUnit)
		{
			if (pUnit->isPillageEspionage() || pUnit->isPillageMarauder() || pUnit->isPillageResearch())
			{
				if (eUnitAI == UNITAI_PILLAGE
				|| eUnitAI == UNITAI_ATTACK
				|| eUnitAI == UNITAI_PARADROP
				|| eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 10;
				}
				else iTemp += 8;
			}
			else iTemp++;
		}
		else iTemp++;
	}

	if (kUnitCombat.isPillageOnVictory())
	{
		if (pUnit)
		{
			if (pUnit->isPillageEspionage() || pUnit->isPillageMarauder() || pUnit->isPillageResearch())
			{
				if (eUnitAI == UNITAI_PILLAGE
				|| eUnitAI == UNITAI_ATTACK
				|| eUnitAI == UNITAI_PARADROP
				|| eUnitAI == UNITAI_INFILTRATOR)
				{
					iTemp += 20;
				}
				else iTemp += 12;
			}
			else iTemp += 4;
		}
		else iTemp += 4;
	}

	if (kUnitCombat.isPillageResearch())
	{
		if (pUnit)
		{
			if (pUnit->isPillageOnMove() || pUnit->isPillageOnVictory())
			{
				if (eUnitAI == UNITAI_PILLAGE)
				{
					iTemp += 10;
				}
				else if ((eUnitAI == UNITAI_ATTACK) ||
						(eUnitAI == UNITAI_PARADROP) ||
						(eUnitAI == UNITAI_INFILTRATOR))
				{
					iTemp += 8;
				}
				else
				{
					iTemp += 4;
				}
			}
			else
			{
				iTemp += 2;
			}
		}
		else
		{
			iTemp += 2;
		}
	}

	if (pUnit)
	{
		if (pUnit->maxMoves() > 1)
		{
			iTemp *= 100;
		}
	}
	iValue += iTemp;

	iTemp = kUnitCombat.getAirCombatLimitChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getCelebrityHappy();
	int iTempTemp = 0;
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER))
		{
			if (pUnit)
			{
				CvPlot* pUnitPlot = pUnit->plot();
				CvCity* pCity = pUnitPlot->getPlotCity();
				if (pCity != NULL)
				{
					if ((pCity->happyLevel()) < (pCity->unhappyLevel()))
					{
						iTempTemp = (iTemp * 5);
					}
					else
					{
						iTempTemp = (iTemp / 100);
					}
				}
			}
			else
			{
				iTempTemp /= 100;
			}
		}
		else
		{
			iTempTemp /= 100;
		}
		iValue += iTempTemp;
	}

	iTemp = kUnitCombat.getCollateralDamageLimitChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getCollateralDamageMaxUnitsChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (15 * iTemp);
		}
	}

	iTemp = kUnitCombat.getCombatLimitChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
			{
				iTemp *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iTemp /= 4;
				}
				iValue += iTemp;
			}
			else
			{
				int iCombatLimit = pUnit->combatLimit();
				int iCombatLimitValue = iCombatLimit - 100;
				int iCombatLimitValueProcessed = std::min(iTemp, iCombatLimitValue);
				iCombatLimitValueProcessed *= 3;
				if (!(eUnitAI == UNITAI_COLLATERAL))
				{
					iCombatLimitValueProcessed /= 4;
				}

				iValue += iCombatLimitValueProcessed;
			}
		}
		else if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 3);
		}
	}

	iTemp = kUnitCombat.getExtraDropRange();
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 20 * iTemp;
		}
	}

	iTemp = kUnitCombat.getSurvivorChance();
	if (iTemp > 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 5 * iTemp;
		}
		else
		{
			iValue += 2 * iTemp;
		}
	}

	//TBHEAL review
	//isNoSelfHeal()
	if (kUnitCombat.isNoSelfHeal())
	{
		if (pUnit)
		{
			if (!pUnit->hasNoSelfHeal())
			{
				iValue -= 50;
			}
		}
		else
		{
			iValue -= 25;
		}
	}

	iTemp = kUnitCombat.getMaxHPChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getExtraMaxHP();
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY_LEMMING) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER))
		{
			iTemp *= 15;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 10;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getSelfHealModifier();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 10;
		}
		else if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER))
		{
			iTemp *= 5;
		}
		else
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getEnemyHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iTemp *= 4;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_COLLATERAL) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_DEFENSE_AIR))
		{
			iTemp *= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getNeutralHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 3;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 2;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getFriendlyHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp *= 2;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_DEFENSE_AIR) ||
				(eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_HUNTER_ESCORT) ||
				(eUnitAI == UNITAI_EXPLORE))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getVictoryHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getSelfHealModifierTotal();
			if (pUnit->hasNoSelfHeal())
			{
				iTemp = 0;
			}
		}

		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_DEFENSE_AIR) ||
			(eUnitAI == UNITAI_CARRIER_AIR) ||
			(eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 5;
		}

		iValue += iTemp;
	}


	CvPlot* pPlot = NULL;
	if (pUnit)
	{
		pPlot = pUnit->plot();
	}

	iTemp = kUnitCombat.getNumHealSupport();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp += pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 20;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 5;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getSameTileHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 6;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp /= 2;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getAdjacentTileHealChange();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->getNumHealSupportTotal();
		}

		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 5;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 1;
		}
		else
		{
			iTemp /= 3;
		}

		iValue += iTemp;
	}

	iTemp = kUnitCombat.getVictoryStackHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = (pUnit->getSameTileHeal() * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).isHealsAs())
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + kUnitCombat.getNumHealSupport());
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getVictoryAdjacentHeal();
	if (iTemp > 0)
	{
		if (pUnit)
		{
			int iBoost = (pUnit->getAdjacentTileHeal() * 2);
			for (int iK = 0; iK < GC.getNumUnitCombatInfos(); iK++)
			{
				if (GC.getUnitCombatInfo((UnitCombatTypes)iK).isHealsAs())
				{
					UnitCombatTypes eHealUnitCombat = (UnitCombatTypes)iK;
					iBoost += pUnit->getHealUnitCombatTypeAdjacentTotal(eHealUnitCombat);
				}
			}
			iBoost *= (pUnit->getNumHealSupportTotal() + kUnitCombat.getNumHealSupport());
			iTemp += iBoost;
		}
		if ((eUnitAI == UNITAI_HEALER) ||
			(eUnitAI == UNITAI_HEALER_SEA))
		{
			iTemp *= 2;
		}
		else if ((eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iTemp *= 2;
			iTemp /= 3;
		}
		else
		{
			iTemp /= 4;
		}
		iValue += iTemp;
	}

	if (kUnitCombat.isAlwaysHeal())
	{
		if ((eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 35;
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
				(eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_PARADROP))
		{
			iValue += 15;
		}
		else
		{
			iValue += 8;
		}
	}
	//

	if (kUnitCombat.isAmphib())
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 25;
		}
		else
		{
			iValue++;
		}
	}

	if (kUnitCombat.isRiver())
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += 15;
		}
		else
		{
			iValue++;
		}
	}

	if (kUnitCombat.isEnemyRoute())
	{
		if (eUnitAI == UNITAI_PILLAGE)
		{
			iValue += (50 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
				   (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (30 + (4 * iMoves));
		}
		else if ((eUnitAI == UNITAI_PARADROP) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (20 + (4 * iMoves));
		}
		else
		{
			iValue += (4 * iMoves);
		}
	}

	if (kUnitCombat.isHillsDoubleMove())
	{
		if (eUnitAI == UNITAI_EXPLORE ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 50;
		}
		else
		{
			iValue += 10;
		}
	}

	if (kUnitCombat.isCanMovePeaks() && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 35;
	}
	if (kUnitCombat.isCanLeadThroughPeaks() && !(GET_TEAM(getTeam()).isCanPassPeaks()))
	{
		iValue += 75;
	}

	if (kUnitCombat.isImmuneToFirstStrikes()
		&& (pUnit == NULL || !pUnit->immuneToFirstStrikes()))
	{
		if ((eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_ESCORT))
		{
			iValue += 25;
		}
		else if ((eUnitAI == UNITAI_ATTACK))
		{
			iValue += 15;
		}
		else
		{
			iValue += 10;
		}
	}

	iTemp = kUnitCombat.getInsidiousnessChange();
	if (iTemp != 0)
	{
		//TB Must update here as soon as I can improve on AI - not sure about pirates yet.
		if (eUnitAI == UNITAI_INFILTRATOR)
		{
			iTemp *= 10;
		}
		else if (/*(eUnitAI == UNITAI_PIRATE_SEA) ||*/
			(eUnitAI == UNITAI_PILLAGE))
		{
			iTemp *= 2;
		}
		else
		{
			iTemp *= -1;
		}
		if (pUnit)
		{
			if (pUnit->plot() != NULL)
			{
				CvCity* pCity = pUnit->plot()->getPlotCity();
				if (pCity != NULL && pCity->getOwner() != pUnit->getOwner())
				{
					iTemp *= 2;
				}
			}
		}
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getInvestigationChange();
	if (iTemp != 0)
	{
		//TB: Do I need another AI for just this?  hmm...
		//Perhaps work in some temporary situational value at least since buildups will likely apply
		//And only the best at this on the tile will make a difference.
		if (eUnitAI == UNITAI_PROPERTY_CONTROL ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			iTemp *= 2;
			if (pUnit)
			{
				if (pUnit->plot() != NULL)
				{
					CvCity* pCity = pUnit->plot()->getPlotCity();
					if (pCity != NULL)
					{
						int iNumCriminals = pUnit->plot()->getNumCriminals();
						if (eUnitAI == UNITAI_INVESTIGATOR)
						{
							iNumCriminals += 10;
						}
						iTemp += iTemp * (iNumCriminals * iNumCriminals);
					}
				}
			}
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getVisibilityChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_SEE_INVISIBLE) ||
			(eUnitAI == UNITAI_SEE_INVISIBLE_SEA))
		{
			iValue += (iTemp * 50);
		}
		else if ((eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE))
		{
			iValue += (iTemp * 40);
		}
		else if (eUnitAI == UNITAI_PIRATE_SEA)
		{
			iValue += (iTemp * 20);
		}
	}

	iTemp = kUnitCombat.getMovesChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_ASSAULT_SEA) ||
				(eUnitAI == UNITAI_SETTLER_SEA) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_HEALER) ||
				(eUnitAI == UNITAI_HEALER_SEA) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 40);
		}
		else
		{
			iValue += (iTemp * 25);
		}
	}

	iTemp = kUnitCombat.getMoveDiscountChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_PILLAGE) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_EXPLORE_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_SETTLER_SEA) ||
			(eUnitAI == UNITAI_HEALER_SEA) ||
			(eUnitAI == UNITAI_ESCORT) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 20);
		}
		else
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kUnitCombat.getAirRangeChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR ||
			eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 20);
		}
		else if (eUnitAI == UNITAI_DEFENSE_AIR)
		{
			iValue += (iTemp * 10);
		}
	}

	iTemp = kUnitCombat.getInterceptChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_DEFENSE_AIR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_ESCORT_SEA)
		{
			iValue += (iTemp * 4);
		}
		else if (eUnitAI == UNITAI_CITY_SPECIAL || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kUnitCombat.getEvasionChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_AIR || eUnitAI == UNITAI_CARRIER_AIR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 10);
		}
	}

	iTemp = kUnitCombat.getFirstStrikesChange() * 2;
	iTemp += kUnitCombat.getChanceFirstStrikesChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_CITY_SPECIAL) ||
				(eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_ESCORT) ||
				(eUnitAI == UNITAI_ESCORT_SEA))
		{
			iTemp *= 25;
			iExtra = pUnit ? pUnit->getExtraChanceFirstStrikes() + pUnit->getExtraFirstStrikes() * 2 : kUnit.getChanceFirstStrikes() + kUnit.getFirstStrikes() * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp * 5);
		}
	}


	iTemp = kUnitCombat.getStealthStrikesChange() * 2;
	int iInvisFactor = 0;
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				for (int i = 0; i < GC.getNumInvisibleInfos(); i++)
				{
					iInvisFactor += pUnit->invisibilityIntensityTotal((InvisibleTypes)i);
				}
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
				(eUnitAI == UNITAI_INVESTIGATOR) ||
				(eUnitAI == UNITAI_INFILTRATOR))
		{
			iTemp *= 25;
			iExtra = pUnit ? pUnit->getExtraStealthStrikes() * 2 : kUnit.getStealthStrikes() * 2;
			iTemp *= 100 + iExtra * 15;
			iTemp /= 100;

			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	iTemp = kUnitCombat.getWithdrawalChange();
	if (iTemp != 0)
	{
		iExtra = kUnit.getWithdrawalProbability() + (pUnit ? pUnit->getExtraWithdrawal() * 4 : 0);
		iTemp *= 100 + iExtra;
		iTemp /= 100;
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_EXPLORE)
		{
			iValue += (iTemp * 4) / 3;
		}
		else if ((eUnitAI == UNITAI_COLLATERAL) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_PILLAGE) ||
			  (eUnitAI == UNITAI_INFILTRATOR) ||
			  (pUnit != NULL && pUnit->getLeaderUnitType() != NO_UNIT))
		{
			iValue += iTemp * 1;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	//TB Combat Mods Begin
	iTemp = kUnitCombat.getPursuitChange();
	if (iTemp != 0)
	{
		iExtra = pUnit ? pUnit->getExtraPursuit() : kUnit.getPursuit();
		iTemp *= 100 + iExtra * 2;
		iTemp /= 100;
		if ((eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			((GC.getGame().isModderGameOption(MODDERGAMEOPTION_DEFENDER_WITHDRAW) && (eUnitAI == UNITAI_ATTACK_SEA)) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_EXPLORE) ||
				(eUnitAI == UNITAI_HUNTER)) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ATTACK))
		{
			iValue += ((iTemp * 4) / 3);
		}
		else if (eUnitAI == UNITAI_INVESTIGATOR ||
			eUnitAI == UNITAI_ESCORT ||
			eUnitAI == UNITAI_SEE_INVISIBLE ||
			eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += iTemp;
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kUnitCombat.getEarlyWithdrawChange();
	int itempwithdraw = 0;
	if ((kUnit.getWithdrawalProbability() + (pUnit == NULL ? 0 : pUnit->getExtraWithdrawal() * 4)) > 0)
	{
		itempwithdraw += ((kUnit.getWithdrawalProbability() + (pUnit == NULL ? 0 : pUnit->getExtraWithdrawal() * 4)) / 2);
	}
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? 0 : (itempwithdraw + pUnit->earlyWithdrawTotal());
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_RESERVE) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_EXPLORE) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_INFILTRATOR))
		{
			iValue += (iTemp * 2);
		}
		else if (eUnitAI == UNITAI_ESCORT)
		{
			iValue -= (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iValue -= kUnitCombat.getUpkeepModifier();

	iTemp = kUnitCombat.getRBombardDamageBase();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getRBombardDamage() : pUnit->rBombardDamage();
		if (pUnit)
		{
			if (pUnit->getBaseRBombardDamage() < kUnitCombat.getRBombardDamageBase())
			{
				iTemp = (kUnitCombat.getRBombardDamageBase() - pUnit->getBaseRBombardDamage());
			}
			else
			{
				iTemp = 0;
			}
		}

		iTemp *= (100 + iExtra);
		iTemp /= 400;
		if (pUnit != NULL && pUnit->canRBombard(true))
		{
			iValue += iTemp;
		}
		else if (pUnit == NULL)
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getRBombardDamageLimitBase();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getRBombardDamageLimit() : pUnit->rBombardDamageLimit();
		if (pUnit)
		{
			if (pUnit->getBaseRBombardDamageLimit() < kUnitCombat.getRBombardDamageLimitBase())
			{
				iTemp = (kUnitCombat.getRBombardDamageLimitBase() - pUnit->getBaseRBombardDamageLimit());
			}
			else
			{
				iTemp = 0;
			}
		}
		iTemp *= (100 + iExtra);
		iTemp /= 400;
		if (pUnit != NULL && pUnit->canRBombard(true))
		{
			iValue += iTemp;
		}
		else if (pUnit == NULL)
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getRBombardDamageMaxUnitsBase();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getRBombardDamageMaxUnits() : pUnit->rBombardDamageMaxUnits();
		if (pUnit)
		{
			if (pUnit->getBaseRBombardDamageMaxUnits() < kUnitCombat.getRBombardDamageMaxUnitsBase())
			{
				iTemp = (kUnitCombat.getRBombardDamageMaxUnitsBase() - pUnit->getBaseRBombardDamageMaxUnits());
			}
			else
			{
				iTemp = 0;
			}
		}
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (pUnit != NULL && pUnit->canRBombard(true))
		{
			iValue += iTemp * 8;
		}
		else if (pUnit == NULL)
		{
			iValue += iTemp * 8;
		}
	}

	iTemp = kUnitCombat.getDCMBombRangeBase();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDCMBombRange() : pUnit->getDCMBombRange();
		if (pUnit)
		{
			if (pUnit->getBaseDCMBombRange() < kUnitCombat.getDCMBombRangeBase())
			{
				iTemp = (kUnitCombat.getDCMBombRangeBase() - pUnit->getBaseDCMBombRange());
			}
			else
			{
				iTemp = 0;
			}
		}
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (pUnit != NULL && pUnit->canRBombard(true))
		{
			iValue += iTemp * 10;
		}
		else if (pUnit == NULL)
		{
			iValue += iTemp * 10;
		}
	}

	iTemp = kUnitCombat.getDCMBombAccuracyBase();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDCMBombAccuracy() : pUnit->getDCMBombAccuracy();
		if (pUnit)
		{
			if (pUnit->getBaseDCMBombAccuracy() < kUnitCombat.getDCMBombAccuracyBase())
			{
				iTemp = (kUnitCombat.getDCMBombAccuracyBase() - pUnit->getBaseDCMBombAccuracy());
			}
			else
			{
				iTemp = 0;
			}
		}
		iTemp *= (100 + iExtra);
		iTemp /= 400;
		if (pUnit != NULL && pUnit->canRBombard(true))
		{
			iValue += iTemp;
		}
		else if (pUnit == NULL)
		{
			iValue += iTemp;
		}
	}
	//TB Notes: assume that our units value Armor and Puncture in a rather generic accross the board value
	//(at least for now.  Attackers should probably favor puncture while defenders would favor armor
	//as armor should come with mobility dampeners (though would likely be included in the eval anyhow...)
	iTemp = kUnitCombat.getArmorChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getArmor() : pUnit->armorTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPunctureChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPuncture() : pUnit->punctureTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getDamageModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDamageModifier() : pUnit->damageModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getDodgeModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDodgeModifier() : pUnit->dodgeTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPrecisionModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPrecisionModifier() : pUnit->precisionTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getCriticalModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCriticalModifier() : pUnit->criticalModifierTotal();
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPowerShotsChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPowerShotCombatModifier() : pUnit->powerShotCombatModifierTotal();
		iExtra += pUnit == NULL ? kUnit.getPowerShotCriticalModifier() : pUnit->powerShotCriticalModifierTotal();
		iExtra += pUnit == NULL ? kUnit.getPowerShotPunctureModifier() : pUnit->powerShotPunctureModifierTotal();
		iExtra += pUnit == NULL ? kUnit.getPowerShotPrecisionModifier() : pUnit->powerShotPrecisionModifierTotal();
		iExtra += kUnitCombat.getPowerShotCombatModifierChange() == 0 ? 0 : kUnitCombat.getPowerShotCombatModifierChange();
		iExtra += kUnitCombat.getPowerShotCriticalModifierChange() == 0 ? 0 : kUnitCombat.getPowerShotCriticalModifierChange();
		iExtra += kUnitCombat.getPowerShotPunctureModifierChange() == 0 ? 0 : kUnitCombat.getPowerShotPunctureModifierChange();
		iExtra += kUnitCombat.getPowerShotPrecisionModifierChange() == 0 ? 0 : kUnitCombat.getPowerShotPrecisionModifierChange();
		iTemp *= (100 + iExtra);
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPowerShotCombatModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPowerShots() : pUnit->powerShotsTotal();
		iTemp *= iExtra;
		iTemp /= 5;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPowerShotCriticalModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPowerShots() : pUnit->powerShotsTotal();
		iTemp *= iExtra;
		iTemp /= 5;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPowerShotPrecisionModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPowerShots() : pUnit->powerShotsTotal();
		iTemp *= iExtra;
		iTemp /= 4;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getPowerShotPunctureModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getPowerShots() : pUnit->powerShotsTotal();
		iTemp *= iExtra;
		iTemp /= 4;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getEnduranceChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getEndurance() : pUnit->enduranceTotal();
		iTemp += iExtra;
		iTemp *= 10;
		iValue += iTemp;
	}

	iTemp = kUnitCombat.getOverrunChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
			  (eUnitAI == UNITAI_COLLATERAL))
		{
			iExtra = kUnit.getOverrun() + (pUnit == NULL ? 0 : pUnit->getExtraOverrun() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getRepelChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_ESCORT))
		{
			iExtra = kUnit.getRepel() + (pUnit == NULL ? 0 : pUnit->getExtraRepel() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getFortRepelChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_RESERVE))
		{
			iExtra = kUnit.getFortRepel() + (pUnit == NULL ? 0 : pUnit->getExtraFortRepel() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 110);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getRepelRetriesChange();
	int itemprepel = 0;
	if ((kUnit.getRepel() + (pUnit == NULL ? 0 : pUnit->getExtraRepel() * 4)) > 0)
	{
		itemprepel += ((kUnit.getRepel() + (pUnit == NULL ? 0 : pUnit->getExtraRepel() * 4)) / 2);
	}
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? 0 : (itemprepel + pUnit->repelRetriesTotal());
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 25);
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getUnyieldingChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
			  (eUnitAI == UNITAI_PILLAGE_COUNTER) ||
			  (eUnitAI == UNITAI_HUNTER) ||
			  (eUnitAI == UNITAI_HUNTER_ESCORT) ||
			  (eUnitAI == UNITAI_GREAT_HUNTER) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_HEALER) ||
			  (eUnitAI == UNITAI_PROPERTY_CONTROL))
		{
			iExtra = kUnit.getUnyielding() + (pUnit == NULL ? 0 : pUnit->getExtraUnyielding() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 110);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getKnockbackChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iExtra = kUnit.getKnockback() + (pUnit == NULL ? 0 : pUnit->getExtraKnockback() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getKnockbackRetriesChange();
	int itempknockback = 0;
	if ((kUnit.getKnockback() + (pUnit == NULL ? 0 : pUnit->getExtraKnockback() * 4)) > 0)
	{
		itempknockback += ((kUnit.getKnockback() + (pUnit == NULL ? 0 : pUnit->getExtraKnockback() * 4)) / 2);
	}
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? 0 : (itempknockback + pUnit->knockbackRetriesTotal());
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += (iTemp * 25);
		}
		else
		{
			iValue += iTemp;
		}
	}

	for (int iI = 0; iI < kUnitCombat.getNumFlankingStrengthbyUnitCombatTypesChange(); iI++)
	{
		iTemp = kUnitCombat.getFlankingStrengthbyUnitCombatTypeChange(iI).iModifier;

		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER || eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY)
			{
				iExtra = kUnit.getFlankingStrengthbyUnitCombatType(iI) + (pUnit ? 2*pUnit->getExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)iI) : 0);
				iValue += iTemp * (100 + iExtra) / 125;
			}
			else
			{
				iValue += iTemp / 10;
			}
		}
	}

	iTemp = kUnitCombat.getUnnerveChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_PILLAGE))
		{
			iExtra = kUnit.getUnnerve() + (pUnit == NULL ? 0 : pUnit->getExtraUnnerve() * 2);
			iValue += ((iTemp / 2) * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getEncloseChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY) ||
			  (eUnitAI == UNITAI_PILLAGE))
		{
			iExtra = kUnit.getEnclose() + (pUnit == NULL ? 0 : pUnit->getExtraEnclose() * 2);
			iValue += ((iTemp * 5) * (100 + iExtra) / 100);
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getLungeChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY ||
				  (eUnitAI == UNITAI_PILLAGE)))
		{
			iExtra = kUnit.getLunge() + (pUnit == NULL ? 0 : pUnit->getExtraLunge() * 2);
			iValue += (iTemp * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getDynamicDefenseChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_RESERVE) ||
			  (eUnitAI == UNITAI_HEALER) ||
			  (eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			  (eUnitAI == UNITAI_RESERVE_SEA) ||
			  (eUnitAI == UNITAI_ESCORT_SEA) ||
			  (eUnitAI == UNITAI_ESCORT))
		{
			iExtra = kUnit.getDynamicDefense() + (pUnit == NULL ? 0 : pUnit->getExtraDynamicDefense() * 2);
			iValue += (iTemp * (100 + iExtra) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	if (kUnitCombat.isStampedeChange())
	{
		iValue -= 25;
	}

	if (kUnitCombat.isMakesDamageCold())
	{
		iValue += 25;
	}

	if (kUnitCombat.isAddsColdImmunity())
	{
		iValue += 25;
	}

#ifdef BATTLEWORN
	iTemp = kUnitCombat.getStrAdjperAttChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getWithdrawAdjperAttChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			  (eUnitAI == UNITAI_ATTACK_CITY))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}

	iTemp = kUnitCombat.getStrAdjperDefChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_COUNTER) ||
			  (eUnitAI == UNITAI_CITY_COUNTER) ||
			  (eUnitAI == UNITAI_ESCORT) ||
			  (eUnitAI == UNITAI_ESCORT_SEA))
		{
			iValue += iTemp * 5;
		}
		else
		{
			iValue += iTemp;
		}
	}
#endif // BATTLEWORN

#ifdef OUTBREAKS_AND_AFFLICTIONS
	for (int iI = 0; iI < kUnitCombat.getNumAfflictOnAttackChangeTypes(); ++iI)
	{
		if (kUnitCombat.getAfflictOnAttackChangeType(iI).eAfflictionLine > 0)
		{
			iTemp = kUnitCombat.getAfflictOnAttackChangeType(iI).iProbabilityChange;

			int iTempWithdraw = 0;

			if (pUnit == NULL ? 0 : pUnit->withdrawalProbability() > 0)
			{
				iTempWithdraw += pUnit->withdrawalProbability();
			}
			if (pUnit == NULL ? 0 : pUnit->earlyWithdrawTotal() > 0)
			{
				iTempWithdraw += ((iTempWithdraw * pUnit->earlyWithdrawTotal()) / 100);
			}

			iTemp += ((iTemp * iTempWithdraw) / 100);

			int iImmediate = kUnitCombat.getAfflictOnAttackChangeType(iI).iImmediate;
			iImmediate *= 25;

			iTemp += iImmediate;

			iValue += iTemp;
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_OUTBREAKS_AND_AFFLICTIONS))
	{
		for (int iI = 0; iI < kUnitCombat.getNumCureAfflictionChangeTypes(); ++iI)
		{
			if (kUnitCombat.getCureAfflictionChangeType(iI) > 0)
			{
				PromotionLineTypes eAfflictionLine = ((PromotionLineTypes)kUnitCombat.getCureAfflictionChangeType(iI));
				iTemp = 10;
				int iAfflictionCount = getPlayerWideAfflictionCount(eAfflictionLine);
				int iPlotAfflictedCount = 0;
				if (pUnit)
				{
					CvPlot* pUnitPlot = pUnit->plot();
					iPlotAfflictedCount = pUnitPlot->getNumAfflictedUnits(pUnit->getOwner(), eAfflictionLine);
				}
				iPlotAfflictedCount *= 100;
				iAfflictionCount += ((iAfflictionCount * iPlotAfflictedCount) / 100);
				iTemp *= iAfflictionCount;

				iValue += iTemp;
			}
		}

		for (int iI = 0; iI < kUnitCombat.getNumAfflictionFortitudeChangeModifiers(); ++iI)
		{
			iValue += (kUnitCombat.getAfflictionFortitudeChangeModifier(iI).iModifier);
		}

		iTemp = kUnitCombat.getFortitudeChange();
		if (iTemp != 0)
		{
			iExtra = kUnit.getFortitude() + (pUnit == NULL ? 0 : pUnit->getExtraFortitude());
			iTemp *= (100 + iExtra);
			iTemp /= 100;
			iValue += ((iTemp * 3) / 4);
		}

		for (int iI = 0; iI < GC.getNumPropertyInfos(); iI++)
		{
			iTemp = kUnitCombat.getAidChange(iI);
			if (iTemp != 0)
			{
				iExtra = kUnit.getAidChange(iI) + (pUnit == NULL ? 0 : pUnit->extraAidChange((PropertyTypes)iI));
				iTemp *= (100 + iExtra);
				iTemp /= 100;
				iValue += ((iTemp * 3) / 4);
			}
		}
	}
#endif // OUTBREAKS_AND_AFFLICTIONS

#ifdef STRENGTH_IN_NUMBERS
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_STRENGTH_IN_NUMBERS))
	{
		iTemp = kUnitCombat.getFrontSupportPercentChange();
		if (iTemp != 0)
		{
			iExtra = pUnit == NULL ? kUnit.getFrontSupportPercent() : pUnit->getExtraFrontSupportPercent();
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			iValue += iTemp * 4;
		}

		iTemp = kUnitCombat.getShortRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iExtra = pUnit == NULL ? kUnit.getShortRangeSupportPercent() : pUnit->getExtraShortRangeSupportPercent();
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			iValue += iTemp * 4;
		}

		iTemp = kUnitCombat.getMediumRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iExtra = pUnit == NULL ? kUnit.getMediumRangeSupportPercent() : pUnit->getExtraMediumRangeSupportPercent();
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			iValue += iTemp * 4;
		}

		iTemp = kUnitCombat.getLongRangeSupportPercentChange();
		if (iTemp != 0)
		{
			iExtra = pUnit == NULL ? kUnit.getLongRangeSupportPercent() : pUnit->getExtraLongRangeSupportPercent();
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			iValue += iTemp * 4;
		}

		iTemp = kUnitCombat.getFlankSupportPercentChange();
		if (iTemp != 0)
		{
			iExtra = pUnit == NULL ? kUnit.getFlankSupportPercent() : pUnit->getExtraFlankSupportPercent();
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			iValue += iTemp * 4;
		}
}
#endif // STRENGTH_IN_NUMBERS

	iTemp = kUnitCombat.getRoundStunProbChange();
	if (iTemp != 0)
	{
		iExtra = kUnit.getRoundStunProb();
		if (pUnit) iExtra += pUnit->getExtraRoundStunProb();

		// 2x strengthens synergy
		iTemp *= 2 * (100 + iExtra);
		iTemp /= 100;
		iValue += iTemp * 2;
	}
	//TB Combat Mods End

	iTemp = kUnitCombat.getCollateralDamageChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getCollateralDamage() : pUnit->getExtraCollateralDamage(); //collateral has no strong synergy (not like retreat)
		iTemp *= (100 + iExtra);
		iTemp /= 100;

		if (eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 1);
		}
		else if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += ((iTemp * 2) / 3);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getBombardRateChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	//Breakdown
	iTemp = kUnitCombat.getBreakdownChanceChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp * 2);
		}
		else if (pUnit != NULL && pUnit->canAttack())
		{
			iTemp *= pUnit->breakdownDamageTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += (iTemp * 10);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	iTemp = kUnitCombat.getBreakdownDamageChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp * 2);
		}
		else if (pUnit != NULL && pUnit->canAttack())
		{
			iTemp *= pUnit->breakdownChanceTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_ATTACK_CITY)
			{
				iValue += (iTemp * 100);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	iTemp = kUnitCombat.getTauntChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iTemp *= pUnit->tauntTotal();
			iTemp /= 100;
			if (eUnitAI == UNITAI_EXPLORE ||
				eUnitAI == UNITAI_ESCORT)
			{
				iValue += (iTemp * 25);
			}
			else
			{
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = kUnitCombat.getStrengthModifier();
	if (iTemp != 0)
	{
		iValue += iTemp * 2;
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
	{
		if (kUnitCombat.getQualityBase() > -10)
		{
			iTemp = kUnitCombat.getQualityBase() - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}

		if (kUnitCombat.getGroupBase() > -10)
		{
			iTemp = kUnitCombat.getGroupBase() - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}

		if (kUnitCombat.getSizeBase() > -10)
		{
			iTemp = kUnitCombat.getSizeBase() - 5;
			if (iTemp != 0)
			{
				iValue += iTemp * 200;
			}
		}
	}

	iTemp = kUnitCombat.getCombatPercent();
	if (iTemp != 0)
	{
		//@MOD Commanders: combat promotion value
		if (eUnitAI == UNITAI_GENERAL)
		{
			iValue += (iTemp * 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_CITY_DEFENSE) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else if (eUnitAI == UNITAI_PILLAGE ||
				eUnitAI == UNITAI_SEE_INVISIBLE ||
				eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_SEE_INVISIBLE_SEA)
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iInvisFactor = 0;
	iTemp = kUnitCombat.getStealthCombatModifierChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (!GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
			{
				if ((InvisibleTypes)pUnit->getInvisibleType() != NO_INVISIBLE)
				{
					iInvisFactor = 3;
				}
			}
			else
			{
				for (int i = 0; i < GC.getNumInvisibleInfos(); i++)
				{
					iInvisFactor += pUnit->invisibilityIntensityTotal((InvisibleTypes)i);
				}
			}
		}
		else
		{
			iInvisFactor = 1;
		}


		if ((eUnitAI == UNITAI_ANIMAL) ||
			  (eUnitAI == UNITAI_ATTACK) ||
				(eUnitAI == UNITAI_PILLAGE) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_HUNTER) ||
				(eUnitAI == UNITAI_GREAT_HUNTER) ||
				(eUnitAI == UNITAI_PROPERTY_CONTROL))
		{
			iValue += ((iTemp * iInvisFactor) / 2);
		}
		else
		{
			iValue += ((iTemp * (iInvisFactor - 1)) / 2);
		}
	}

	//TB Combat Mod
	iTemp = kUnitCombat.getStrengthChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			if (pUnit->baseCombatStrNonGranular() < 10)
			{
				int ievaluation = ((11 - pUnit->baseCombatStrNonGranular()) * 30);
				iValue += (iTemp * ievaluation);
			}
			else
			{
				iValue += (iTemp * 30);
			}
		}
		else
		{
			iValue += (iTemp * 20);
		}
	}

	int iRank = 0;
	iTemp = kUnitCombat.getCombatModifierPerSizeMoreChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->sizeRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getCombatModifierPerSizeLessChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->sizeRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getCombatModifierPerVolumeMoreChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = 6 - pUnit->groupRank();
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR) ||
				(eUnitAI == UNITAI_ESCORT))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	iRank = 0;
	iTemp = kUnitCombat.getCombatModifierPerVolumeLessChange();
	if (iTemp != 0)
	{
		if (pUnit)
		{
			iRank = pUnit->groupRank() - 4;
			iTemp *= std::max(0, iRank);
		}
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
				(eUnitAI == UNITAI_ATTACK_SEA) ||
				(eUnitAI == UNITAI_PARADROP) ||
				(eUnitAI == UNITAI_PIRATE_SEA) ||
				(eUnitAI == UNITAI_RESERVE_SEA) ||
				(eUnitAI == UNITAI_ESCORT_SEA) ||
				(eUnitAI == UNITAI_CARRIER_SEA) ||
				(eUnitAI == UNITAI_ATTACK_AIR) ||
				(eUnitAI == UNITAI_CARRIER_AIR))
		{
			iValue += (iTemp * 3);
		}
		else
		{
			iValue += (iTemp * 1);
		}
	}

	if (pUnit)
	{
		if (kUnitCombat.isRemoveStampede() && pUnit->mayStampede())
		{
			iValue += 25;
		}

		if (kUnitCombat.isMakesDamageNotCold() && pUnit->dealsColdDamage())
		{
			iValue -= 25;
		}

		if (kUnitCombat.isRemovesColdImmunity() && pUnit->hasImmunitytoColdDamage())
		{
			iValue -= 25;
		}

		if (kUnitCombat.getAnimalIgnoresBordersChange() > 0 && pUnit->isAnimal() && !GC.getGame().isOption(GAMEOPTION_ANIMAL_STAY_OUT))
		{
			iValue += 50;
		}

		if (kUnitCombat.getNoDefensiveBonusChange() > 0)
		{
			if (!pUnit->noDefensiveBonus())
			{
				iValue -= 50;
			}
			else iValue -= 5;
		}
		else if (kUnitCombat.getNoDefensiveBonusChange() < 0)
		{
			if (pUnit->noDefensiveBonus())
			{
				iValue += 50;
			}
			else iValue += 5;
		}

		if (kUnitCombat.isOnslaughtChange())
		{
			iValue += 75;
		}

		if (pUnit->canAttack())
		{
			if (kUnitCombat.isAttackOnlyCitiesSubtract())
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue += 50;
				}
				else iValue += 1;
			}

			if (kUnitCombat.isAttackOnlyCitiesAdd())
			{
				if (pUnit->canAttackOnlyCities())
				{
					iValue -= 10;
				}
				else iValue -= 50;
			}

			if (!pUnit->canAttackOnlyCities())
			{
				if (kUnitCombat.isIgnoreNoEntryLevelAdd())
				{
					if (pUnit->canIgnoreNoEntryLevel())
					{
						iValue += 5;
					}
					else iValue += 20;
				}

				if (kUnitCombat.isIgnoreNoEntryLevelSubtract())
				{
					if (pUnit->canIgnoreNoEntryLevel())
					{
						iValue -= 20;
					}
					else iValue -= 5;
				}
			}
		}

		if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL))
		{
			if (kUnitCombat.isIgnoreZoneofControlAdd())
			{
				if (pUnit->canIgnoreZoneofControl())
				{
					iValue += 5;
				}
				else iValue += 25;
			}
			if (kUnitCombat.isIgnoreZoneofControlSubtract())
			{
				if (pUnit->canIgnoreZoneofControl())
				{
					iValue -= 25;
				}
				else iValue -= 5;
			}
		}

		if (kUnitCombat.isFliesToMoveAdd())
		{
			if (pUnit->canFliesToMove())
			{
				iValue += 5;
			}
			else iValue += 100;
		}

		if (kUnitCombat.isFliesToMoveSubtract())
		{
			if (pUnit->canFliesToMove())
			{
				iValue -= 75;
			}
			else iValue -= 5;
		}
	}


	for (int iI = 0; iI < kUnitCombat.getNumWithdrawVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getWithdrawVSUnitCombatTypeChange(iI).iModifier;
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_FIGHT_OR_FLIGHT))
	{
		for (int iI = 0; iI < kUnitCombat.getNumPursuitVSUnitCombatTypesChange(); iI++)
		{
			iValue += kUnitCombat.getPursuitVSUnitCombatTypeChange(iI).iModifier;
		}
	}

	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HEART_OF_WAR))
	{
		for (int iI = 0; iI < kUnitCombat.getNumRepelVSUnitCombatTypesChange(); iI++)
		{
			iValue += kUnitCombat.getRepelVSUnitCombatTypeChange(iI).iModifier;
		}

		for (int iI = 0; iI < kUnitCombat.getNumKnockbackVSUnitCombatTypesChange(); iI++)
		{
			iValue += kUnitCombat.getKnockbackVSUnitCombatTypeChange(iI).iModifier;
		}
	}

	for (int iI = 0; iI < kUnitCombat.getNumPunctureVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getPunctureVSUnitCombatTypeChange(iI).iModifier;
	}


	for (int iI = 0; iI < kUnitCombat.getNumArmorVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getArmorVSUnitCombatTypeChange(iI).iModifier;
	}

	for (int iI = 0; iI < kUnitCombat.getNumDodgeVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getDodgeVSUnitCombatTypeChange(iI).iModifier;
	}

	for (int iI = 0; iI < kUnitCombat.getNumPrecisionVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getPrecisionVSUnitCombatTypeChange(iI).iModifier;
	}

	for (int iI = 0; iI < kUnitCombat.getNumCriticalVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getCriticalVSUnitCombatTypeChange(iI).iModifier;
	}

	for (int iI = 0; iI < kUnitCombat.getNumRoundStunVSUnitCombatTypesChange(); iI++)
	{
		iValue += kUnitCombat.getRoundStunVSUnitCombatTypeChange(iI).iModifier;
	}

	//TB Combat Mods
	//TB Modification note:adjusted City Attack promo value to balance better against withdraw promos for city attack ai units.
	if (eUnitAI == UNITAI_ATTACK || eUnitAI == UNITAI_ATTACK_CITY || eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
	{
		const int iCityAttack = kUnitCombat.getCityAttackPercent();

		if (iCityAttack != 0)
		{
			if (eUnitAI == UNITAI_ATTACK_CITY
			||  eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
			{
				iValue += iCityAttack * 4;
			}
			else if (eUnitAI == UNITAI_ATTACK)
			{
				iValue += iCityAttack;
			}
		}
	}

	iTemp = kUnitCombat.getCityDefensePercent();
	if (iTemp != 0)
	{
		iExtra = kUnit.getCityDefenseModifier() + (pUnit == NULL ? 0 : pUnit->getExtraCityDefensePercent() * 2);
		iTemp *= 100 + iExtra;
		iTemp /= 100;
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			  (eUnitAI == UNITAI_CITY_SPECIAL) ||
			  (eUnitAI == UNITAI_CITY_COUNTER))
		{
			iValue += (iTemp * 4);
		}
		else if ((eUnitAI == UNITAI_HEALER) ||
			  (eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			  (eUnitAI == UNITAI_INVESTIGATOR))
		{
			iValue += iTemp / 2;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = kUnitCombat.getHillsAttackPercent();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getHillsAttackModifier() : pUnit->getExtraHillsAttackPercent();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kUnitCombat.getHillsDefensePercent();
	if (iTemp != 0)
	{
		iExtra = (kUnit.getHillsDefenseModifier() + (pUnit == NULL ? 0 : pUnit->getExtraHillsDefensePercent() * 2));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_CITY_COUNTER ||
			eUnitAI == UNITAI_ESCORT)
		{
			if (pUnit != NULL && pUnit->plot()->isCity() && pUnit->plot()->isHills())
			{
				iValue += (iTemp * 4) / 3;
			}
		}
		else if (eUnitAI == UNITAI_COUNTER)
		{
			if (pUnit != NULL && pUnit->plot()->isHills())
			{
				iValue += (iTemp / 4);
			}
			else
			{
				iValue++;
			}
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	//WorkRateMod
	iTemp = kUnitCombat.getHillsWorkPercent();

	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kUnitCombat.getPeaksWorkPercent();
	//Note: this could use some further development (particularly in ensuring that the unit CAN go onto peaks)
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_WORKER)
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue++;
		}
	}

	//Team Project (3)
	iTemp = kUnitCombat.getCaptureProbabilityModifierChange();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_INVESTIGATOR) ||
			(pUnit != NULL && pUnit->canAttack()))
		{
			iValue += iTemp * 3;
		}
		else if (pUnit != NULL && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kUnitCombat.getCaptureResistanceModifierChange();
	if (iTemp != 0)
	{
		if (pUnit != NULL && pUnit->canFight())
		{
			iValue += iTemp;
		}
		else if (eUnitAI == UNITAI_INVESTIGATOR ||
				eUnitAI == UNITAI_ESCORT)
		{
			iValue += iTemp;
		}
		else
		{
			iValue++;
		}
	}

	iTemp = kUnitCombat.getRevoltProtection();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL) ||
			(eUnitAI == UNITAI_PROPERTY_CONTROL) ||
			(eUnitAI == UNITAI_INVESTIGATOR))
		{
			if (pUnit != NULL && pUnit->plot() != NULL && pUnit->getX() != INVALID_PLOT_COORD && pUnit->plot()->isCity())
			{
				PlayerTypes eCultureOwner = pUnit->plot()->calculateCulturalOwner();
				// High weight for cities being threatened with culture revolution
				if (eCultureOwner != NO_PLAYER && GET_PLAYER(eCultureOwner).getTeam() != getTeam())
				{
					iValue += iTemp * 15;
				}
			}
		}
	}

	iTemp = kUnitCombat.getCollateralDamageProtection();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_CITY_DEFENSE) ||
			(eUnitAI == UNITAI_CITY_COUNTER) ||
			(eUnitAI == UNITAI_CITY_SPECIAL))
		{
			iValue += (iTemp / 3);
		}
		else if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = kUnitCombat.getPillageChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_PIRATE_SEA ||
			eUnitAI == UNITAI_INFILTRATOR)
		{
			iValue += (iTemp * 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = kUnitCombat.getUpgradeDiscount();
	if (iTemp != 0)
	{
		iValue += (iTemp / 16);
	}

	iTemp = kUnitCombat.getExperiencePercent();
	if (iTemp != 0)
	{
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_RESERVE_SEA) ||
			(eUnitAI == UNITAI_ESCORT_SEA) ||
			(eUnitAI == UNITAI_CARRIER_SEA) ||
			(eUnitAI == UNITAI_MISSILE_CARRIER_SEA))
		{
			iValue += (iTemp * 1);
		}
		else
		{
			iValue += (iTemp / 2);
		}
	}

	iTemp = kUnitCombat.getKamikazePercent();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_ATTACK_CITY)
		{
			iValue += (iTemp / 16);
		}
		else
		{
			iValue += (iTemp / 64);
		}
	}

	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
	{
		if (kUnitCombat.getWithdrawOnTerrainTypeChange(iI) != 0)
		{
			iTemp = kUnitCombat.getWithdrawOnTerrainTypeChange(iI);
			int iTerrainWeight;
			//ls612: Weight values by Terrain in area
			//General form of the calculation is to change a value of 'x' (old version) to '(x * iTerrainWeight)/250'
			//The 250 normalizes the calculation to give the old values for a terrain that covers 25% of the area
			if (pUnit)
			{
				int iNumRevealedAreaTiles = pUnit->area()->getNumRevealedTiles(getTeam());
				int	iNumRevealedAreaThisTerrain = pUnit->area()->getNumRevealedTerrainTiles(getTeam(), (TerrainTypes)iI);
				iTerrainWeight = (1000 * iNumRevealedAreaThisTerrain) / iNumRevealedAreaTiles;
			}
			else
			{
				iTerrainWeight = 1000;
			}

			if (iTemp != 0)
			{

				iExtra = pUnit ? pUnit->getExtraWithdrawOnTerrainType((TerrainTypes)iI) : kUnit.getWithdrawOnTerrainType(iI);
				iTemp *= (100 + iExtra);
				iTemp /= 100;

				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_COLLATERAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_EXPLORE_SEA ||
					eUnitAI == UNITAI_CARRIER_SEA ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_SUBDUED_ANIMAL)
				{
					if (pUnit != NULL && pUnit->plot()->getTerrainType() == (TerrainTypes)iI && pUnit->withdrawalProbability() > 0)
					{
						iValue += (iTerrainWeight * iTemp) / 2500;
					}
					else
					{
						iValue += (iTerrainWeight * iTemp) / 5000;
					}
				}
				else
				{
					if (pUnit != NULL && pUnit->withdrawalProbability() > 0)
					{
						iValue += (iTerrainWeight * iTemp) / 75500;
					}
					else
					{
						iValue += (iTerrainWeight * iTemp) / 12500;
					}
				}
			}
		}
	}

	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
	{
		if (kUnitCombat.getWithdrawOnFeatureTypeChange(iI))
		{
			iTemp = kUnitCombat.getWithdrawOnFeatureTypeChange(iI);
			if (iTemp != 0)
			{
				//ls612: Feature Weights
				//General form of the calculation is to change a value of 'x' (old version) to '(x * iFeatureWeight)/100'
				//The 100 normalizes the calculation to give the old values for a feature that covers 10% of the area - note
				//that we use 10% for features where we used 25% for terrains because most plots don't have features at all
				int iFeatureWeight;
				if (pUnit)
				{
					int iNumRevealedAreaFeatures = std::max(1, pUnit->area()->getNumRevealedTiles(getTeam()));
					int iNumRevealedAreaFeatureTiles = pUnit->area()->getNumRevealedFeatureTiles(getTeam(), (FeatureTypes)iI);
					iFeatureWeight = ((iNumRevealedAreaFeatureTiles * 1000) / iNumRevealedAreaFeatures);
				}
				else
				{
					iFeatureWeight = 1000;
				}

				if (iTemp != 0)
				{

					iExtra = pUnit ? pUnit->getExtraWithdrawOnFeatureType((FeatureTypes)iI, true) : kUnit.getWithdrawOnFeatureType(iI);
					iTemp *= (100 + iExtra);
					iTemp /= 100;

					if (eUnitAI == UNITAI_ANIMAL ||
						eUnitAI == UNITAI_COLLATERAL ||
						eUnitAI == UNITAI_PILLAGE ||
						eUnitAI == UNITAI_EXPLORE ||
						eUnitAI == UNITAI_ATTACK_SEA ||
						eUnitAI == UNITAI_EXPLORE_SEA ||
						eUnitAI == UNITAI_CARRIER_SEA ||
						eUnitAI == UNITAI_PIRATE_SEA ||
						eUnitAI == UNITAI_SUBDUED_ANIMAL)
					{
						if (pUnit != NULL && pUnit->plot()->getFeatureType() == (FeatureTypes)iI && pUnit->withdrawalProbability() > 0)
						{
							iValue += (iFeatureWeight * iTemp) / 1200;
						}
						else
						{
							iValue += (iFeatureWeight * iTemp) / 1600;
						}
					}
					else
					{
						if (pUnit != NULL && pUnit->withdrawalProbability() > 0)
						{
							iValue += (iFeatureWeight * iTemp) / 2000;
						}
						else
						{
							iValue++;
						}
					}
				}
			}
		}
	}

	for (int iI = 0; iI < kUnitCombat.getNumUnitCombatChangeModifiers(); iI++)
	{
		iValue += kUnitCombat.getUnitCombatChangeModifier(iI).iModifier;
	}

	for (int iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		iTemp = kUnitCombat.getDomainModifierPercent(iI);
		if (iTemp != 0)
		{
			if (eUnitAI == UNITAI_COUNTER)
			{
				iValue += (iTemp * 1);
			}
			else if ((eUnitAI == UNITAI_ATTACK) ||
					   (eUnitAI == UNITAI_RESERVE))
			{
				iValue += (iTemp / 2);
			}
			else
			{
				iValue += (iTemp / 8);
			}
		}
	}

	// TB Combat Mods Begin

	//TB Rudimentary to get us started
	if (GC.getGame().isOption(GAMEOPTION_COMBAT_HIDE_SEEK))
	{
		for (int iI = 0; iI < GC.getNumInvisibleInfos(); iI++)
		{
			iTemp = kUnitCombat.getVisibilityIntensityChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)iI);
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 35);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 15);
				}
				else
				{
					iValue += (iTemp * 10);
				}
			}
			iTemp = kUnitCombat.getInvisibilityIntensityChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)iI);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 30);
				}
				else
				{
					iValue += (iTemp * 5);
				}
			}
			iTemp = kUnitCombat.getVisibilityIntensityRangeChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					if (pUnit->visibilityIntensityTotal((InvisibleTypes)iI) > 0)
					{
						iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)iI);
					}
					else
					{
						iTemp = 0;
					}
				}
				if (eUnitAI == UNITAI_SEE_INVISIBLE ||
					eUnitAI == UNITAI_SEE_INVISIBLE_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER)
				{
					iValue += (iTemp * 35);
				}
				else if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL ||
					eUnitAI == UNITAI_ESCORT)
				{
					iValue += (iTemp * 15);
				}
				else
				{
					iValue += (iTemp * 10);
				}
			}
			iTemp = kUnitCombat.getVisibilityIntensitySameTileChangeType(iI);
			if (iTemp != 0)
			{
				if (pUnit)
				{
					if (pUnit->visibilityIntensityTotal((InvisibleTypes)iI) > 0)
					{
						iTemp += pUnit->visibilityIntensityTotal((InvisibleTypes)iI);
					}
					else
					{
						iTemp = 0;
					}
				}
				if (eUnitAI == UNITAI_COUNTER ||
					eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_ESCORT_SEA ||
					eUnitAI == UNITAI_PILLAGE_COUNTER ||
					eUnitAI == UNITAI_HUNTER_ESCORT ||
					eUnitAI == UNITAI_PROPERTY_CONTROL)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 2);
				}
			}
		}
		//VERY rudimentary
		for (int iI = 0; iI < kUnitCombat.getNumInvisibleTerrainChanges(); iI++)
		{
			iTemp = kUnitCombat.getInvisibleTerrainChange(iI).iIntensity;
			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kUnitCombat.getInvisibleTerrainChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kUnitCombat.getNumInvisibleFeatureChanges(); iI++)
		{
			iTemp = kUnitCombat.getInvisibleFeatureChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kUnitCombat.getInvisibleFeatureChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 4);
				}
			}
		}
		for (int iI = 0; iI < kUnitCombat.getNumInvisibleImprovementChanges(); iI++)
		{
			iTemp = kUnitCombat.getInvisibleImprovementChange(iI).iIntensity;

			if (iTemp != 0)
			{
				if (pUnit)
				{
					iTemp += pUnit->invisibilityIntensityTotal((InvisibleTypes)kUnitCombat.getInvisibleImprovementChange(iI).eInvisible);
				}
				if (eUnitAI == UNITAI_ANIMAL ||
					eUnitAI == UNITAI_PILLAGE ||
					eUnitAI == UNITAI_EXPLORE ||
					eUnitAI == UNITAI_PIRATE_SEA ||
					eUnitAI == UNITAI_ATTACK_SEA ||
					eUnitAI == UNITAI_MISSILE_CARRIER_SEA ||
					eUnitAI == UNITAI_HUNTER ||
					eUnitAI == UNITAI_GREAT_HUNTER ||
					eUnitAI == UNITAI_PROPERTY_CONTROL_SEA ||
					eUnitAI == UNITAI_INFILTRATOR)
				{
					iValue += (iTemp * 10);
				}
				else
				{
					iValue += (iTemp * 3);
				}
			}
		}
	}

	iTemp = kUnitCombat.getAttackCombatModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getAttackCombatModifier() : pUnit->getExtraAttackCombatModifier();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((eUnitAI == UNITAI_ATTACK) ||
			(eUnitAI == UNITAI_ATTACK_CITY) ||
			(eUnitAI == UNITAI_COLLATERAL) ||
			(eUnitAI == UNITAI_ATTACK_SEA) ||
			(eUnitAI == UNITAI_ASSAULT_SEA) ||
			(eUnitAI == UNITAI_PIRATE_SEA) ||
			(eUnitAI == UNITAI_ATTACK_AIR) ||
			(eUnitAI == UNITAI_HUNTER) ||
			(eUnitAI == UNITAI_HUNTER_ESCORT) ||
			(eUnitAI == UNITAI_GREAT_HUNTER) ||
			(eUnitAI == UNITAI_ANIMAL))
		{
			iValue += (iTemp * 2);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kUnitCombat.getDefenseCombatModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getDefenseCombatModifier() : pUnit->getExtraDefenseCombatModifier();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;

		int iMultiplier = 2;

		if (eUnitAI == UNITAI_RESERVE ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_ESCORT)
		{
			iValue += (iTemp * iMultiplier);
		}
		else
		{
			iValue += (iTemp);
		}
	}

	iTemp = kUnitCombat.getVSBarbsChange();
	if (iTemp != 0)
	{
		if (eUnitAI == UNITAI_COUNTER ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_CITY_COUNTER ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_EXPLORE_SEA ||
			eUnitAI == UNITAI_EXPLORE ||
			eUnitAI == UNITAI_PILLAGE_COUNTER ||
			eUnitAI == UNITAI_HUNTER ||
			eUnitAI == UNITAI_HUNTER_ESCORT ||
			eUnitAI == UNITAI_GREAT_HUNTER ||
			eUnitAI == UNITAI_ESCORT_SEA ||
			eUnitAI == UNITAI_ESCORT)
		{
			int iEraFactor = 10 - (int)getCurrentEra();
			iTemp *= iEraFactor;
			iTemp /= 9;
			if (pUnit != NULL && !pUnit->isHominid())
			{
				iValue += iTemp;
			}
		}
	}

	iTemp = kUnitCombat.getReligiousCombatModifierChange();
	if (iTemp != 0)
	{
		iExtra = pUnit == NULL ? kUnit.getReligiousCombatModifier() : pUnit->getExtraReligiousCombatModifier();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;

		if (eUnitAI == UNITAI_ATTACK ||
			eUnitAI == UNITAI_ATTACK_CITY ||
			eUnitAI == UNITAI_COLLATERAL ||
			eUnitAI == UNITAI_PILLAGE ||
			eUnitAI == UNITAI_RESERVE ||
			eUnitAI == UNITAI_COUNTER ||
			eUnitAI == UNITAI_CITY_DEFENSE ||
			eUnitAI == UNITAI_ATTACK_SEA ||
			eUnitAI == UNITAI_RESERVE_SEA ||
			eUnitAI == UNITAI_ASSAULT_SEA ||
			eUnitAI == UNITAI_ATTACK_CITY_LEMMING)
		{
			iTemp *= 2;
			iValue += iTemp;
		}
	}

	// TB Combat Mods Begin
	if (GC.getGame().isOption(GAMEOPTION_UNSUPPORTED_ZONE_OF_CONTROL) && kUnitCombat.isZoneOfControl())
	{
		iValue += 250;
	}

	return iValue;
}

TechTypes CvPlayerAI::AI_bestReligiousTech(int iMaxPathLength, TechTypes eIgnoreTech, AdvisorTypes eIgnoreAdvisor)
{
	PROFILE("CvPlayerAI::AI_bestReligiousTech");

	int iBestValue = 0;
	TechTypes eBestTech = NO_TECH;

	for (int iI = 0; iI < GC.getNumTechInfos(); iI++)
	{
		const TechTypes eTechX = static_cast<TechTypes>(iI);

		if ((eIgnoreTech == NO_TECH || eTechX != eIgnoreTech)
		&& (eIgnoreAdvisor == NO_ADVISOR || GC.getTechInfo(eTechX).getAdvisorType() != eIgnoreAdvisor)
		&& canResearch(eTechX, false)
		&& GC.getTechInfo(eTechX).getEra() <= getCurrentEra())
		{
			const int iPathLength = findPathLength(eTechX, false);

			if (iPathLength <= iMaxPathLength)
			{
				const int iValue = AI_religiousTechValue(eTechX) / std::max(1, iPathLength);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestTech = eTechX;
				}
			}
		}
	}
	return eBestTech;
}

int CvPlayerAI::AI_religiousTechValue(TechTypes eTech) const
{
	PROFILE_FUNC();

	int iReligionValue = 0;

	if (canFoundReligion())
	{
		for (int iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
		{
			const TechTypes eReligionTech = GC.getReligionInfo((ReligionTypes)iJ).getTechPrereq();
			if (eReligionTech == eTech)
			{
				if (!(GC.getGame().isReligionSlotTaken((ReligionTypes)iJ)))
				{
					if (!GC.getGame().isOption(GAMEOPTION_RELIGION_PICK))
					{
						const ReligionTypes eFavorite = (ReligionTypes)GC.getLeaderHeadInfo(getLeaderType()).getFavoriteReligion();
						if (eFavorite != NO_RELIGION)
						{
							if (iJ == eFavorite)
							{
								iReligionValue += 1000;
							}
							else
							{
								iReligionValue += 500;
							}
						}
					}
					iReligionValue += 250;
				}
				if (GC.getGame().isOption(GAMEOPTION_RELIGION_DIVINE_PROPHETS))
				{
					if (GC.getGame().countKnownTechNumTeams(eTech) < 1)
					{
						iReligionValue += 250;
					}
				}
			}
		}

		if (iReligionValue > 0)
		{
			if (AI_isDoVictoryStrategy(AI_VICTORY_CULTURE1))
			{
				iReligionValue += 500;
			}
		}
	}

	return iReligionValue;
}

void CvPlayerAI::AI_doMilitaryProductionCity()
{
	PROFILE_FUNC();

	//invalidate cache
	m_iMilitaryProductionCityCount = -1;
	m_iNavalMilitaryProductionCityCount = -1;

	algo::for_each(cities(), CvCity::fn::AI_setMilitaryProductionCity(false));
	algo::for_each(cities(), CvCity::fn::AI_setNavalMilitaryProductionCity(false));

	if (getNumCities() < 4)
	{
		return;
	}

	int iNumMilitaryProdCitiesNeeded = getNumCities() / 4;

	if (GET_TEAM(getTeam()).isAtWar())
	{
		iNumMilitaryProdCitiesNeeded += getNumCities() / 4;
	}
	else if (GET_TEAM(getTeam()).hasWarPlan(true))
	{
		iNumMilitaryProdCitiesNeeded += getNumCities() / 8;
	}
	if (AI_isFinancialTrouble())
	{
		iNumMilitaryProdCitiesNeeded = std::max(1, iNumMilitaryProdCitiesNeeded / 2);
	}

	for (int iPass = iNumMilitaryProdCitiesNeeded; iPass > 0; iPass--)
	{
		foreach_(CvCity * pLoopCity, cities())
		{
			if (pLoopCity->AI_getMilitaryProductionRateRank() == iPass)
			{
				pLoopCity->AI_setMilitaryProductionCity(true);
			}
			if (pLoopCity->AI_getNavalMilitaryProductionRateRank() == iPass)
			{
				pLoopCity->AI_setNavalMilitaryProductionCity(true);
			}
		}
	}
}

int CvPlayerAI::AI_getMilitaryProductionCityCount() const
{
	if (m_iMilitaryProductionCityCount != -1)
	{
		return m_iMilitaryProductionCityCount;
	}

	const int iCount = algo::count_if(cities(), CvCity::fn::AI_isMilitaryProductionCity());

	m_iMilitaryProductionCityCount = iCount;
	return iCount;
}

int CvPlayerAI::AI_getNavalMilitaryProductionCityCount() const
{
	if (m_iNavalMilitaryProductionCityCount != -1)
	{
		return m_iNavalMilitaryProductionCityCount;
	}

	const int iCount = algo::count_if(cities(), CvCity::fn::AI_isNavalMilitaryProductionCity());

	m_iNavalMilitaryProductionCityCount = iCount;
	return iCount;
}


int	CvPlayerAI::AI_getNumBuildingsNeeded(BuildingTypes eBuilding, bool bCoastal) const
{
	PROFILE_EXTRA_FUNC();
	// Total needed to have one in every city
	BuildingCountMap::const_iterator itr = m_numBuildingsNeeded.find(eBuilding);

	if (itr == m_numBuildingsNeeded.end())
	{
		int	result = 0;

		foreach_(const CvCity * pLoopCity, cities())
		{
			if ((!bCoastal || pLoopCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize()))
			&& !pLoopCity->hasBuilding(eBuilding))
			{
				result++;
			}
		}
		{
			m_numBuildingsNeeded[eBuilding] = result;
		}
		return result;
	}
	return itr->second;
}

void CvPlayerAI::AI_changeNumBuildingsNeeded(BuildingTypes eBuilding, int iChange)
{
	m_numBuildingsNeeded[eBuilding] += iChange;
}

void CvPlayerAI::AI_noteUnitRecalcNeeded()
{
	bUnitRecalcNeeded = true;
}

void CvPlayerAI::AI_recalculateUnitCounts()
{
	PROFILE_FUNC();

	for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
	{
		AI_changeNumAIUnits((UnitAITypes)iI, -AI_getNumAIUnits((UnitAITypes)iI));

		foreach_(CvArea * pLoopArea, GC.getMap().areas())
		{
			pLoopArea->changeNumAIUnits(m_eID, (UnitAITypes)iI, -pLoopArea->getNumAIUnits(m_eID, (UnitAITypes)iI));
		}
	}

	foreach_(const CvUnit * pLoopUnit, units())
	{
		const UnitAITypes eAIType = pLoopUnit->AI_getUnitAIType();

		if (NO_UNITAI != eAIType)
		{
			AI_changeNumAIUnits(eAIType, 1);

			pLoopUnit->area()->changeNumAIUnits(m_eID, eAIType, 1);
		}
	}

	bUnitRecalcNeeded = false;
}

int CvPlayerAI::AI_calculateAverageLocalInstability() const
{
	PROFILE_EXTRA_FUNC();
	int	result = 0;
	int iNum = 0;

	foreach_(const CvCity * pLoopCity, cities())
	{
		//	Count big cities more than small ones to some exent as a revolution there hurts more
		int	iWeight = (pLoopCity->getPopulation() < 6 ? 1 : 2);

		result += iWeight * pLoopCity->getLocalRevIndex();
		iNum += iWeight;
	}

	return (iNum == 0 ? 0 : result / iNum);
}

int CvPlayerAI::AI_calculateAverageCityDistance() const
{
	PROFILE_EXTRA_FUNC();
	int	result = 0;
	int iNum = 0;
	const CvCity* pCapital = getCapitalCity();

	if (pCapital != NULL)
	{
		foreach_(const CvCity * pLoopCity, cities())
		{
			if (pLoopCity != pCapital)
			{
				int iCapitalDistance = ::plotDistance(pLoopCity->getX(), pLoopCity->getY(), pCapital->getX(), pCapital->getY());
				//	Count big cities more than small ones to some exent as a revolution there hurts more
				int	iWeight = (pLoopCity->getPopulation() < 6 ? 1 : 2);

				result += iWeight * iCapitalDistance;
				iNum += iWeight;
			}
		}
	}

	return (iNum == 0 ? 0 : result / iNum);
}

void CvPlayerAI::AI_noteWarStatusChange(TeamTypes eTeam, bool bAtWar)
{
	//	Cancel any existing cached beeline tech target if war status changes
	m_eBestResearchTarget = NO_TECH;
}


// Evaluate a building we are considering building here in terms of its effect on properties
int CvPlayerAI::heritagePropertiesValue(const CvHeritageInfo& heritage) const
{
	PROFILE_EXTRA_FUNC();

	const CvCityAI* pCapital = static_cast<CvCityAI*>(getCapitalCity());
	if (!pCapital)
	{
		return 0;
	}
	// Evaluate building properties
	std::map<int, int> effectivePropertyChanges;

	foreach_(const CvPropertySource * pSource, heritage.getPropertyManipulators()->getSources())
	{
		if (pSource->getType() == PROPERTYSOURCE_CONSTANT)
		{
			// Convert to an effective absolute amount by looking at the steady state value given current
			const PropertyTypes eProperty = pSource->getProperty();
			// Only count half the unit source as we want to encourage building sources over unit ones
			const int iCurrentSourceSize = (
				  pCapital->getTotalBuildingSourcedProperty(eProperty)
				+ pCapital->getTotalUnitSourcedProperty(eProperty) / 2
				+ pCapital->getPropertyNonBuildingSource(eProperty)
			);
			const int iNewSourceSize = iCurrentSourceSize + static_cast<const CvPropertySourceConstant*>(pSource)->getAmountPerTurn(getGameObject());
			const int iDecayPercent = pCapital->getPropertyDecay(eProperty);

			// Steady state occurs at a level where the decay removes as much per turn as the sources add
			//	Decay can be 0 if the current level is below the threshold at which decay cuts in, so for the
			//	purposes of calculation just treat this as very slow decay
			const int iCurrentSteadyStateLevel = (100 * iCurrentSourceSize) / std::max(1, iDecayPercent);
			const int iNewSteadyStateLevel = (100 * iNewSourceSize) / std::max(1, iDecayPercent);

			std::map<int, int>::iterator itr = effectivePropertyChanges.find(eProperty);
			if (itr == effectivePropertyChanges.end())
			{
				effectivePropertyChanges[eProperty] = (iNewSteadyStateLevel - iCurrentSteadyStateLevel);
			}
			else
			{
				itr->second += (iNewSteadyStateLevel - iCurrentSteadyStateLevel);
			}
		}
	}

	int iValue = 0;
	for (std::map<int, int>::const_iterator itr = effectivePropertyChanges.begin(); itr != effectivePropertyChanges.end(); ++itr)
	{
		iValue += pCapital->getPropertySourceValue((PropertyTypes)itr->first, itr->second);
	}
	return iValue;
}

int CvPlayerAI::AI_heritageValue(const HeritageTypes eType) const
{
	PROFILE_FUNC();

	const CvHeritageInfo& heritage = GC.getHeritageInfo(eType);

	int iValue = 0;
	{
		const EraTypes eEra = getCurrentEra();

		foreach_(const EraCommerceArray& pair, heritage.getEraCommerceChanges100())
		{
			if (eEra >= pair.first)
			{
				for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
				{
					iValue += pair.second[(CommerceTypes)iI];
				}
			}
		}
	}
	iValue += heritagePropertiesValue(heritage);

	return std::max(0, iValue);
}
