Scriptname AmusementGamesNative Hidden
{The Papyrus side of what the plugin (AmusementGames.dll) does: Nuka-World's sounds, the scoreboard digits, and the
weapon you play some of the games with. Forms come in as their ids (Game.GetForm): refs the plugin made and forms
with no script of their own can't be called on from the plugin directly.}

; a sound descriptor played at a ref: its playing instance, to stop it later (0 if it didn't play)
Int Function PlaySound(Int aiSound, Int aiAt) Global
	Sound s = Game.GetForm(aiSound) as Sound
	ObjectReference at = Game.GetForm(aiAt) as ObjectReference
	If s && at
		Return s.Play(at)
	EndIf
	Return 0
EndFunction

Function StopSound(Int aiInstance) Global
	If aiInstance != 0
		Sound.StopInstance(aiInstance)
	EndIf
EndFunction

; a scoreboard digit (DLC04_Nukacade_Numbers) set to 0-9, as Nuka-World's DLC04ArcadeScoreNumber does it
Bool Function ShowDigit(Int aiDigit, Int aiNumber) Global
	ObjectReference d = Game.GetForm(aiDigit) as ObjectReference
	If d && d.Is3DLoaded()
		Return d.PlayAnimation("Num0" + aiNumber)
	EndIf
	Return False
EndFunction

; the game's weapon in your hands (added first if you haven't one: with aiMod fitted, its iron sights in place of a
; scope), with ammo for it, drawn. Returns the weapon you had out (its id, 0 for none), to be given back after
Int Function ArmPlayer(Int aiWeapon, Bool abAdd, Int aiAmmo, Int aiAmmoCount, Int aiMod) Global
	Actor p = Game.GetPlayer()
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	If !w
		Return 0
	EndIf
	Weapon had = p.GetEquippedWeapon()
	If abAdd
		p.AddItem(w, 1, True)
		ObjectMod m = Game.GetForm(aiMod) as ObjectMod
		If m
			p.AttachModToInventoryItem(w, m)
		EndIf
	EndIf
	Form ammo = Game.GetForm(aiAmmo)
	If ammo && aiAmmoCount > 0
		p.AddItem(ammo, aiAmmoCount, True)
	EndIf
	p.EquipItem(w, False, True)
	p.DrawWeapon()
	If had && had != w
		Return had.GetFormID()
	EndIf
	Return 0
EndFunction

; ...and back: the game's weapon put away (taken back if it was lent), the ammo lent taken back (what's left of it),
; the weapon you had out back in your hands
Function DisarmPlayer(Int aiWeapon, Bool abRemove, Int aiAmmo, Int aiAmmoCount, Int aiHad) Global
	Actor p = Game.GetPlayer()
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	If w
		p.UnequipItem(w, False, True)
		If abRemove && p.GetItemCount(w) > 0
			p.RemoveItem(w, 1, True)
		EndIf
	EndIf
	Form ammo = Game.GetForm(aiAmmo)
	If ammo && aiAmmoCount > 0
		Int left = p.GetItemCount(ammo)
		If left > aiAmmoCount
			left = aiAmmoCount
		EndIf
		If left > 0
			p.RemoveItem(ammo, left, True)
		EndIf
	EndIf
	Form had = Game.GetForm(aiHad)
	If had
		p.EquipItem(had, False, True)
	EndIf
EndFunction

; a machine's prop (a weapon lying on its counter): not to be picked up, its mod fitted (the rifle's iron sights)
Function Prop(Int aiRef, Int aiMod) Global
	ObjectReference r = Game.GetForm(aiRef) as ObjectReference
	If r
		r.BlockActivation(True, True)
		ObjectMod m = Game.GetForm(aiMod) as ObjectMod
		If m
			r.AttachMod(m)
		EndIf
	EndIf
EndFunction

; a machine's gun to lie on its counter: placed whole (PlaceAtMe builds a weapon's parts; a ref made from the plugin
; gets only its first piece), not to be picked up, its mod fitted (the rifle's iron sights). Its id, for the plugin to
; lay it down where it goes
Int Function PlaceWeapon(Int aiAt, Int aiWeapon, Int aiMod) Global
	ObjectReference at = Game.GetForm(aiAt) as ObjectReference
	Form w = Game.GetForm(aiWeapon)
	If !at || !w
		Return 0
	EndIf
	ObjectReference r = at.PlaceAtMe(w, 1, False, False, True)
	If !r
		Return 0
	EndIf
	r.BlockActivation(True, True)
	ObjectMod m = Game.GetForm(aiMod) as ObjectMod
	If m
		r.AttachMod(m)
	EndIf
	Return r.GetFormID()
EndFunction

; something put in a container (a settlement's workshop: its games' takings)
Function AddTo(Int aiRef, Int aiItem, Int aiCount) Global
	ObjectReference r = Game.GetForm(aiRef) as ObjectReference
	Form item = Game.GetForm(aiItem)
	If r && item && aiCount > 0
		r.AddItem(item, aiCount, True)
	EndIf
EndFunction

; ---------------------------------------------------------------- settlers at the games
; walked over by AmusementGamesQuest's package (as Wasteland Wagers walks its players), then the plugin takes it from
; there: NpcStartGallery, NpcWalk (again, if they stall), NpcTakeUp (there: the rifle up, held), NpcEnd

AmusementGamesQuest Function TheQuest() Global
	Return Game.GetFormFromFile(0x80F, "AmusementGames.esl") as AmusementGamesQuest
EndFunction

Keyword Function LinkSpot() Global
	Return Game.GetFormFromFile(0x80D, "AmusementGames.esl") as Keyword
EndFunction

; a settler to Bandit Roundup: not if they're busy; walking to the marker. 1: on their way; anything else: not coming
; (for the plugin's log: 10 missing forms, 11 dead, 12 fighting, 13 following you, 14 knocked out, 15 doing something
; for you, 16x sitting or at a workstation (160 + sit state), 17 bleeding out, 18 the walk couldn't be given them,
; 20 not a person: a dog, brahmin or robot, 22 dealing at a Wasteland Wagers table)
Int Function NpcStartGallery(Int aiActor, Int aiWeapon, Int aiMarker) Global
	Actor a = Game.GetForm(aiActor) as Actor
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	ObjectReference m = Game.GetForm(aiMarker) as ObjectReference
	AmusementGamesQuest q = TheQuest()
	Keyword k = LinkSpot()
	If !a || !m || !q || !k  ; (no weapon: a game played with the hands)
		Return 10
	ElseIf a.IsDead()
		Return 11
	ElseIf a.IsInCombat()
		Return 12
	ElseIf a.IsPlayerTeammate()
		Return 13
	ElseIf a.IsUnconscious()
		Return 14
	ElseIf a.IsDoingFavor()
		Return 15
	ElseIf a.GetSitState() != 0
		Return 160 + a.GetSitState()
	ElseIf a.IsBleedingOut()
		Return 17
	ElseIf !a.HasKeyword(Game.GetFormFromFile(0x00013794, "Fallout4.esm") as Keyword)
		Return 20
	EndIf
	; Wasteland Wagers: whoever's really sat at its tables was turned away above (sitting); its dealers are left be.
	; Anyone else it still has a link on (a bettor between spins, or one it's left linked and standing about after a
	; table's game, sometimes with their AI still off) is let go by its own Leave first, then called over
	Keyword wagersSpot = Game.GetFormFromFile(0x806, "WastelandWagers.esl") as Keyword
	Keyword wagersSeat = Game.GetFormFromFile(0x848, "WastelandWagers.esl") as Keyword
	If (wagersSpot && a.GetLinkedRef(wagersSpot)) || (wagersSeat && a.GetLinkedRef(wagersSeat))
		ScriptObject wq = Game.GetFormFromFile(0x808, "WastelandWagers.esl").CastAs("WastelandWagersQuest")
		If wq
			RefCollectionAlias dealers = wq.GetPropertyValue("Dealers") as RefCollectionAlias
			If dealers && dealers.Find(a) >= 0
				Return 22
			EndIf
			Var[] args = new Var[2]
			args[0] = aiActor
			args[1] = wagersSpot.GetFormID()
			wq.CallFunction("Leave", args)
			If wagersSeat
				a.SetLinkedRef(None, wagersSeat)
			EndIf
		EndIf
	EndIf
	a.EnableAI(True)  ; (whatever left it off)
	If !q.Join(aiActor, aiMarker, k.GetFormID())
		Return 18
	EndIf
	Return 1
EndFunction

; ...stalled on the way: the walk sent again
Function NpcWalk(Int aiActor, Int aiMarker, Int aiReset) Global
	AmusementGamesQuest q = TheQuest()
	Keyword k = LinkSpot()
	If q && k
		q.Walk(aiActor, aiMarker, k.GetFormID(), aiReset != 0)
	EndIf
EndFunction

; ...there: put on the spot, facing the targets, the rifle taken up (lent if they've none) and raised to the shoulder.
; Their AI stays on (with it off no idle plays: they froze half way through drawing): the walk package, still linked to
; the spot, keeps them there. 1 ready, 2 ready (the rifle lent), 0 not
Int Function NpcTakeUp(Int aiActor, Int aiWeapon, Int aiMarker, Int aiAimAt, Int aiAmmo) Global
	Actor a = Game.GetForm(aiActor) as Actor
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	ObjectReference m = Game.GetForm(aiMarker) as ObjectReference
	If !a || !m || a.IsDead() || a.IsInCombat()
		Return 0
	EndIf
	; onto the spot, at the height of the ground they're standing on: the spot's own height is the machine's, and where
	; the ground's a little higher there, MoveTo put them a few centimetres into it till the game pushed them back up
	; (after a few seconds: their first shots, sunk)
	m.SetPosition(m.GetPositionX(), m.GetPositionY(), a.GetPositionZ())
	a.MoveTo(m)
	If !w
		Return 1  ; (a game played with the hands: Atomic Rollers, Hoop Shot)
	EndIf
	Int r = 1
	If a.GetItemCount(w) == 0
		; the rifles as you're handed them (added, with their iron sights). The Thirst Zapper built whole (PlaceAtMe):
		; added as a bare record, a settler's has no ammo of its own and never squirts (yours is filled when you take it up)
		If w == Game.GetFormFromFile(0x007BC8, "DLCNukaWorld.esm")
			ObjectReference g = a.PlaceAtMe(w, 1, False, True)
			If g
				a.AddItem(g, 1, True)
			Else
				a.AddItem(w, 1, True)
			EndIf
		Else
			a.AddItem(w, 1, True)
		EndIf
		ObjectMod sights = None
		If w == Game.GetFormFromFile(0x02C8B0, "DLCCoast.esm")
			sights = Game.GetFormFromFile(0x042E6F, "DLCCoast.esm") as ObjectMod   ; (DLC03_mod_LeverGun_Scope_SightsIron)
		ElseIf w == Game.GetFormFromFile(0x04F46A, "Fallout4.esm")
			sights = Game.GetFormFromFile(0x148418, "Fallout4.esm") as ObjectMod   ; (mod_HuntingRifle_Scope_SightsIron)
		EndIf
		If sights
			a.AttachModToInventoryItem(w, sights)
		EndIf
		r = 2
	EndIf
	; its own ammo in their pockets, so it fires what it's made to (the Thirst Zapper's water, not a bullet): a few
	; rounds topped up when they're low (left with them: never more than a pocketful)
	Ammo am = Game.GetForm(aiAmmo) as Ammo
	If am && a.GetItemCount(am) < 10
		a.AddItem(am, 50, True)
	EndIf
	; equipped so it stays (abPreventRemoval): left to the AI, they took up whatever gun of their own they rated best
	a.EquipItem(w, True, True)
	; then their package (it picks the weapon in hand: before, it picked one of their own)
	AmusementGamesQuest q = TheQuest()
	Keyword k = LinkSpot()
	If q && k
		If aiAimAt != 0
			Keyword ak = Game.GetFormFromFile(0x82D, "AmusementGames.esl") as Keyword
			Int akId = 0
			If ak
				akId = ak.GetFormID()
			EndIf
			; (the spot held, aiming at the targets, no other idles or chatter)
			q.Hold(aiActor, aiMarker, k.GetFormID(), aiAimAt, akId, w == Game.GetFormFromFile(0x007BC8, "DLCNukaWorld.esm"))
		Else
			q.HoldDrawn(aiActor, aiMarker, k.GetFormID())  ; (a melee weapon: kept drawn between swings)
		EndIf
	EndIf
	a.DrawWeapon()
	; (no raising it ourselves: their hold package aims it, and a sighted idle on top of that put them in a low crouch,
	; as behind cover, for the first few shots)
	Return r
EndFunction

; Fallout4.esm's own idles for a rifle: raised to the sights (RaiderRifleSightedStart), a shot (RaiderFireSingle), lowered
; (RaiderRifleSightedEnd); their animation events sent straight if the idle won't play
Function Aim(Actor a) Global
	If !a.PlayIdle(Game.GetFormFromFile(0x000199F6, "Fallout4.esm") as Idle)
		a.PlayAnimation("rifleSightedStart")
	EndIf
EndFunction

; ...a shot (they're aiming already: their hold package aims at the targets; drawn and raised again if they've put it
; away): the rifle's own shot animation, which fires it, as at an enemy; fired from them without it only if that won't play. 1: the shot animation, 2: fired without
; it, 0: no one to fire it
Int Function NpcShoot(Int aiActor, Int aiWeapon, Int aiAmmo) Global
	Actor a = Game.GetForm(aiActor) as Actor
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	If !a || !w
		Return 0
	EndIf
	If a.GetEquippedWeapon() != w && a.GetItemCount(w) > 0
		a.EquipItem(w, True, True)  ; (swapped for one of their own: the game's again)
	EndIf
	If !a.IsWeaponDrawn()
		a.DrawWeapon()
		Utility.Wait(0.6)
		Aim(a)
		Utility.Wait(0.25)
	EndIf
	If a.PlayIdle(Game.GetFormFromFile(0x00018E1F, "Fallout4.esm") as Idle)
		Return 1
	EndIf
	a.PlayAnimation("attackStart")  ; (no Weapon.Fire: from a settler whose gun has nothing to fire, it crashed the game)
	Return 2
EndFunction

; ...and let go: out of the quest, AI back on, the lent rifle taken back
Function NpcEnd(Int aiActor, Int aiWeapon, Int aiLent) Global
	Actor a = Game.GetForm(aiActor) as Actor
	If !a
		Return
	EndIf
	; out of any idle still playing (a throw's, which left Hoop Shot settlers standing there after): ActionIdleStop
	a.PlayIdleAction(Game.GetFormFromFile(0x00018BA8, "Fallout4.esm") as Action)
	If !a.PlayIdle(Game.GetFormFromFile(0x00018DEA, "Fallout4.esm") as Idle)  ; (RaiderRifleSightedEnd: the rifle down)
		a.PlayAnimation("rifleSightedEnd")
	EndIf
	AmusementGamesQuest q = TheQuest()
	Keyword k = LinkSpot()
	If q && k
		q.Leave(aiActor, k.GetFormID())
	Else
		a.EnableAI(True)
	EndIf
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	If w && aiLent != 0
		a.UnequipItem(w, False, True)
		a.RemoveItem(w, 1, True)
	EndIf
	a.EvaluatePackage()
EndFunction

; after a load: nobody left in the quest's walk
Function NpcClearAll(Int aiUnused) Global
	AmusementGamesQuest q = TheQuest()
	Keyword k = LinkSpot()
	If q && k
		q.ClearAll(k.GetFormID())
	EndIf
EndFunction

; a settler's moves at the other games: the game's own idle, its animation event sent straight if that won't play.
; 1: the idle, 2: the event
Int Function Act(Actor a, Int aiIdle, Int aiIdle2, String asEvent) Global
	If !a
		Return 0
	EndIf
	If a.PlayIdle(Game.GetFormFromFile(aiIdle, "Fallout4.esm") as Idle)
		Return 1
	EndIf
	If aiIdle2 != 0 && a.PlayIdle(Game.GetFormFromFile(aiIdle2, "Fallout4.esm") as Idle)
		Return 1
	EndIf
	a.PlayAnimation(asEvent)
	Return 2
EndFunction

; Whac-a-Commie: a one-handed swing of the Commie Whacker (drawn again first if it's been put away)
Int Function NpcSwing(Int aiActor, Int aiSpot, Int aiAt, Int aiWeapon) Global
	Actor a = Game.GetForm(aiActor) as Actor
	If !a
		Return 0
	EndIf
	Weapon w = Game.GetForm(aiWeapon) as Weapon
	If w && a.GetEquippedWeapon() != w && a.GetItemCount(w) > 0
		a.EquipItem(w, True, True)  ; (the mallet, not a weapon of their own)
	EndIf
	; at the machine, facing it: back on their spot if a swing's carried them off it, turned to the head they're after
	ObjectReference spot = Game.GetForm(aiSpot) as ObjectReference
	ObjectReference at = Game.GetForm(aiAt) as ObjectReference
	If spot && a.GetDistance(spot) > 20.0
		a.MoveTo(spot)
	EndIf
	If at
		Float off = a.GetHeadingAngle(at)
		If off > 2.0 || off < -2.0
			a.SetAngle(0.0, 0.0, a.GetAngleZ() + off)
		EndIf
	EndIf
	If !a.IsWeaponDrawn()
		a.DrawWeapon()
		Utility.Wait(0.5)
	EndIf
	Return Act(a, 0x00056123, 0x000516EE, "meleeattackStart")  ; MeleeAttackStanding_3rdP, MeleeAttack
EndFunction

; Atomic Rollers: the ball rolled up the lane, a mine's throw (low and underhand); if that won't play without a mine
; in hand, Nuka-World's own smoke bomb toss (an idle of its own, nothing in hand needed). 1 the mine's, 3 the toss, 2
; neither (the event sent)
Int Function NpcRoll(Int aiActor) Global
	Actor a = Game.GetForm(aiActor) as Actor
	If a && a.PlayIdle(Game.GetFormFromFile(0x000831, "AmusementGames.esl") as Idle)
		Return 4  ; (AmusementGamesSkeeballRoll: our own)
	EndIf
	If a && (a.PlayIdle(Game.GetFormFromFile(0x000EE0F0, "Fallout4.esm") as Idle) || a.PlayIdle(Game.GetFormFromFile(0x000EC533, "Fallout4.esm") as Idle))
		Return 1  ; (CharacterMTMineThrow, CharacterWeaponMineThrow)
	EndIf
	Return Toss(a, "mineThrowStart")
EndFunction

; Hoop Shot: the basketball thrown, a grenade's throw; or the smoke bomb toss
Int Function NpcToss(Int aiActor) Global
	Actor a = Game.GetForm(aiActor) as Actor
	If a && a.PlayIdle(Game.GetFormFromFile(0x000830, "AmusementGames.esl") as Idle)
		Return 4  ; (AmusementGamesBasketballShot: our own)
	EndIf
	If a && (a.PlayIdle(Game.GetFormFromFile(0x000EE0F1, "Fallout4.esm") as Idle) || a.PlayIdle(Game.GetFormFromFile(0x000CF927, "Fallout4.esm") as Idle))
		Return 1  ; (CharacterMTGrenadeThrow, CharacterWeaponGrenadeThrow)
	EndIf
	Return Toss(a, "grenadeThrowStart")
EndFunction

Int Function Toss(Actor a, String asEvent) Global
	If !a
		Return 0
	EndIf
	If a.PlayIdle(Game.GetFormFromFile(0x0004A45A, "DLCNukaWorld.esm") as Idle)  ; DLC04LooseSmokeBombThrow
		Return 3
	EndIf
	a.PlayAnimation(asEvent)
	Return 2
EndFunction
