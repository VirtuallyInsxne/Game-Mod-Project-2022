
#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

class rvMonsterGrunt : public idAI {
public:

	CLASS_PROTOTYPE( rvMonsterGrunt );

	rvMonsterGrunt ( void );
	
	void				Spawn					( void );
	void				Save					( idSaveGame *savefile ) const;
	void				Restore					( idRestoreGame *savefile );
	
	virtual void		AdjustHealthByDamage	( int damage );

	void StartBattle(idPlayer* player);

protected:

	rvAIAction			actionMeleeMoveAttack;
	rvAIAction			actionChaingunAttack;

	virtual bool		CheckActions		( void );

	virtual void		OnTacticalChange	( aiTactical_t oldTactical );
	virtual void		OnDeath				( void );

private:

	int					standingMeleeNoAttackTime;
	int					rageThreshold;

	void				RageStart			( void );
	void				RageStop			( void );
	
	// Torso States
	stateResult_t		State_Torso_Enrage		( const stateParms_t& parms );
	stateResult_t		State_Torso_Pain		( const stateParms_t& parms );
	stateResult_t		State_Torso_LeapAttack	( const stateParms_t& parms );

	CLASS_STATES_PROTOTYPE ( rvMonsterGrunt );
};

CLASS_DECLARATION( idAI, rvMonsterGrunt )
END_CLASS

/*
================
rvMonsterGrunt::rvMonsterGrunt
================
*/
rvMonsterGrunt::rvMonsterGrunt ( void ) {
	standingMeleeNoAttackTime = 0;
}

/*
================
rvMonsterGrunt::Spawn
================
*/
void rvMonsterGrunt::Spawn ( void ) {
	rageThreshold = spawnArgs.GetInt ( "health_rageThreshold" );

	if (spawnArgs.GetBool("isCaptured")) {

		attack_stat = spawnArgs.GetInt("attack_stat");
		hp_stat = spawnArgs.GetInt("hp_stat");
		speed_stat = spawnArgs.GetInt("speed_stat");
		def_stat = spawnArgs.GetInt("def_stat");
		exp = spawnArgs.GetInt("exp");
		expNextLevel = spawnArgs.GetInt("expNextLevel");
		level = spawnArgs.GetInt("level");
		isCaptured = spawnArgs.GetBool("isCaptured");
		move1 = spawnArgs.GetString("move1");
		move2 = spawnArgs.GetString("move2");
		move3 = spawnArgs.GetString("move3");
		move4 = spawnArgs.GetString("move4");

	}

	else {

		attack_stat = 2;
		hp_stat = 15;
		speed_stat = 3;
		def_stat = 1;
		exp = 0;
		expNextLevel = 10;
		level = 1;
		move1 = "Tackle";
		move2 = "Slash";
		move3 = "Buff Up";
		move4 = "Tail Whip";

	}

}

/*
================
rvMonsterGrunt::Save
================
*/
void rvMonsterGrunt::Save ( idSaveGame *savefile ) const {
	actionMeleeMoveAttack.Save( savefile );
	actionChaingunAttack.Save( savefile );

	savefile->WriteInt( rageThreshold );
	savefile->WriteInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::Restore
================
*/
void rvMonsterGrunt::Restore ( idRestoreGame *savefile ) {
	actionMeleeMoveAttack.Restore( savefile );
	actionChaingunAttack.Restore( savefile );

	savefile->ReadInt( rageThreshold );
	savefile->ReadInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::RageStart
================
*/
void rvMonsterGrunt::RageStart ( void ) {
	SetShaderParm ( 6, 1 );

	// Disable non-rage actions
	actionEvadeLeft.fl.disabled = true;
	actionEvadeRight.fl.disabled = true;
	
	// Speed up animations
	animator.SetPlaybackRate ( 1.25f );

	// Disable pain
	pain.threshold = 0;

	// Start over with health when enraged
	health = spawnArgs.GetInt ( "health" );
	
	// No more going to rage
	rageThreshold = 0;
}

/*
================
rvMonsterGrunt::RageStop
================
*/
void rvMonsterGrunt::RageStop ( void ) {
	SetShaderParm ( 6, 0 );
}

/*
================
rvMonsterGrunt::CheckActions
================
*/
bool rvMonsterGrunt::CheckActions ( void ) {
	// If our health is below the rage threshold then enrage
	if ( health < rageThreshold ) { 
		PerformAction ( "Torso_Enrage", 4, true );
		return true;
	}

	// Moving melee attack?
	if ( PerformAction ( &actionMeleeMoveAttack, (checkAction_t)&idAI::CheckAction_MeleeAttack, NULL ) ) {
		return true;
	}
	
	// Default actions
	if ( CheckPainActions ( ) ) {
		return true;
	}

	if ( PerformAction ( &actionEvadeLeft,   (checkAction_t)&idAI::CheckAction_EvadeLeft, &actionTimerEvade )			 ||
			PerformAction ( &actionEvadeRight,  (checkAction_t)&idAI::CheckAction_EvadeRight, &actionTimerEvade )			 ||
			PerformAction ( &actionJumpBack,	 (checkAction_t)&idAI::CheckAction_JumpBack, &actionTimerEvade )			 ||
			PerformAction ( &actionLeapAttack,  (checkAction_t)&idAI::CheckAction_LeapAttack )	) {
		return true;
	} else if ( PerformAction ( &actionMeleeAttack, (checkAction_t)&idAI::CheckAction_MeleeAttack ) ) {
		standingMeleeNoAttackTime = 0;
		return true;
	} else {
		if ( actionMeleeAttack.status != rvAIAction::STATUS_FAIL_TIMER
			&& actionMeleeAttack.status != rvAIAction::STATUS_FAIL_EXTERNALTIMER
			&& actionMeleeAttack.status != rvAIAction::STATUS_FAIL_CHANCE )
		{//melee attack fail for any reason other than timer?
			if ( combat.tacticalCurrent == AITACTICAL_MELEE && !move.fl.moving )
			{//special case: we're in tactical melee and we're close enough to think we've reached the enemy, but he's just out of melee range!
				if ( !standingMeleeNoAttackTime )
				{
					standingMeleeNoAttackTime = gameLocal.GetTime();
				}
				else if ( standingMeleeNoAttackTime + 2500 < gameLocal.GetTime() )
				{//we've been standing still and not attacking for at least 2.5 seconds, fall back to ranged attack
					//allow ranged attack
					actionRangedAttack.fl.disabled = false;
				}
			}
		}
		if ( PerformAction ( &actionRangedAttack,(checkAction_t)&idAI::CheckAction_RangedAttack, &actionTimerRangedAttack ) ) {
			return true;
		}
	}
	return false;
}

/*
================
rvMonsterGrunt::OnDeath
================
*/
void rvMonsterGrunt::OnDeath ( void ) {
	RageStop ( );
	return idAI::OnDeath ( );
}

/*
================
rvMonsterGrunt::OnTacticalChange

Enable/Disable the ranged attack based on whether the grunt needs it
================
*/
void rvMonsterGrunt::OnTacticalChange ( aiTactical_t oldTactical ) {
	switch ( combat.tacticalCurrent ) {
		case AITACTICAL_MELEE:
			actionRangedAttack.fl.disabled = true;
			break;

		default:
			actionRangedAttack.fl.disabled = false;
			break;
	}
}

/*
=====================
rvMonsterGrunt::AdjustHealthByDamage
=====================
*/
void rvMonsterGrunt::AdjustHealthByDamage ( int damage ) {
	
	idPlayer* player = gameLocal.GetLocalPlayer();

	if (!isCaptured && player->monsterClass == "") {

		isCaptured = true;
		player->Captured = 1;
		player->playerAtk = attack_stat;
		player->playerDef = def_stat;
		player->playerSpd = speed_stat;
		player->playerHp = hp_stat;
		player->playerExp = exp;
		player->playerExpNextLevel = expNextLevel;
		player->playerCurrentLevel = level;
		player->monsterClass = "monster_grunt";
		player->move1 = move1;
		player->move2 = move2;
		player->move3 = move3;
		player->move4 = move4;

	}

	else if (isCaptured) {

		Hide();
		player->Captured = 0;

	}

	else if (!isCaptured && player->monsterClass != "") {
			idAI::StartBattle(player);

	}
}

/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION ( rvMonsterGrunt )
	STATE ( "Torso_Enrage",		rvMonsterGrunt::State_Torso_Enrage )
	STATE ( "Torso_Pain",		rvMonsterGrunt::State_Torso_Pain )
	STATE ( "Torso_LeapAttack",	rvMonsterGrunt::State_Torso_LeapAttack )
END_CLASS_STATES

/*
================
rvMonsterGrunt::State_Torso_Pain
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Pain ( const stateParms_t& parms ) {
	// Stop streaming pain if its time to get angry
	if ( pain.loopEndTime && health < rageThreshold ) {
		pain.loopEndTime = 0;
	}
	return idAI::State_Torso_Pain ( parms );
}

/*
================
rvMonsterGrunt::State_Torso_Enrage
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Enrage ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			PlayAnim ( ANIMCHANNEL_TORSO, "anger", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, 4 ) ) {
				RageStart ( );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}


/*
================
rvMonsterGrunt::State_Torso_LeapAttack
================
*/
stateResult_t rvMonsterGrunt::State_Torso_LeapAttack ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			lastAttackTime = 0;
			// Play the action animation
			PlayAnim ( ANIMCHANNEL_TORSO, animator.GetAnim ( actionAnimNum )->FullName ( ), parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, parms.blendFrames ) ) {
				// If we missed our leap attack get angry
				if ( !lastAttackTime && rageThreshold ) {
					PostAnimState ( ANIMCHANNEL_TORSO, "Torso_Enrage", parms.blendFrames );
				}
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}
