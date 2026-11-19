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
