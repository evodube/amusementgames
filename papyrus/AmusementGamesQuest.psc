Scriptname AmusementGamesQuest extends Quest
{Amusement Games: settlers walked to a machine to play it. The Players collection carries AmusementGamesWalkToSpot (walk
to their linked ref, keyword AmusementGamesLinkSpot, and face it), which outranks the sandbox packages settlers usually
have (as Wasteland Wagers walks its bettors and dealers). Actors, refs and keywords come in as form IDs.}

RefCollectionAlias Property Players Auto Const Mandatory
RefCollectionAlias Property Shooters Auto Const Mandatory  ; (there with the rifle: AmusementGamesHoldAtSpot)
RefCollectionAlias Property Holders Auto Const Mandatory   ; (there with the mallet: AmusementGamesHoldDrawnAtSpot)

Function StartAndWait()
	Start()
	Int tries = 0
	While IsStarting() && tries < 50
		Utility.Wait(0.1)
		tries += 1
	EndWhile
EndFunction

; into Players, linked to aiTarget: their package walks them there. True if the collection holds them
Bool Function Join(Int aiActor, Int aiTarget, Int aiLink)
	Actor a = Game.GetForm(aiActor) as Actor
	ObjectReference t = Game.GetForm(aiTarget) as ObjectReference
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a || !t || !k
		Return False
	EndIf
	If !IsRunning()
		StartAndWait()
	EndIf
	Players.AddRef(a)
	a.SetLinkedRef(None, k)
	a.EvaluatePackage()
	a.SetLinkedRef(t, k)
	a.EvaluatePackage()
	Return Players.Find(a) >= 0
EndFunction

; the walk sent again (a Travel that's stuck starts over; abReset: their AI reset too)
Function Walk(Int aiActor, Int aiTarget, Int aiLink, Bool abReset)
	Actor a = Game.GetForm(aiActor) as Actor
	ObjectReference t = Game.GetForm(aiTarget) as ObjectReference
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a || !t || !k
		Return
	EndIf
	a.SetLinkedRef(None, k)
	a.EvaluatePackage()
	a.SetLinkedRef(t, k)
	a.EvaluatePackage(abReset)
EndFunction

; there: from the walk to the hold (the spot kept, weapon drawn, nothing else: no hellos, chatter or wandering looks)
Function Hold(Int aiActor, Int aiTarget, Int aiLink, Int aiAimAt, Int aiAimLink)
	Actor a = Game.GetForm(aiActor) as Actor
	ObjectReference t = Game.GetForm(aiTarget) as ObjectReference
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a || !t || !k
		Return
	EndIf
	Players.RemoveRef(a)
	Shooters.AddRef(a)
	a.SetLinkedRef(t, k)
	Keyword ak = Game.GetForm(aiAimLink) as Keyword
	If ak
		a.SetLinkedRef(Game.GetForm(aiAimAt) as ObjectReference, ak)  ; (what they aim at)
	EndIf
	a.SetHeadTracking(False)
	a.EvaluatePackage()
EndFunction

; there with a melee weapon: from the walk to the hold with it drawn (kept out between swings), nothing else
Function HoldDrawn(Int aiActor, Int aiTarget, Int aiLink)
	Actor a = Game.GetForm(aiActor) as Actor
	ObjectReference t = Game.GetForm(aiTarget) as ObjectReference
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a || !t || !k
		Return
	EndIf
	Players.RemoveRef(a)
	Holders.AddRef(a)
	a.SetLinkedRef(t, k)
	a.EvaluatePackage()
EndFunction

; out of Players: their own AI again
Function Leave(Int aiActor, Int aiLink)
	Actor a = Game.GetForm(aiActor) as Actor
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a
		Return
	EndIf
	Players.RemoveRef(a)
	Shooters.RemoveRef(a)
	Holders.RemoveRef(a)
	a.EnableAI(True)
	a.SetHeadTracking(True)
	If k
		a.SetLinkedRef(None, k)
	EndIf
	Keyword ak = Game.GetFormFromFile(0x82D, "AmusementGames.esl") as Keyword
	If ak
		a.SetLinkedRef(None, ak)
	EndIf
	a.EvaluatePackage()
EndFunction

; everyone out (a save loaded: whoever the plugin had walking or playing is gone from its mind)
Function ClearAll(Int aiLink)
	Keyword k = Game.GetForm(aiLink) as Keyword
	ClearCollection(Players, k)
	ClearCollection(Shooters, k)
	ClearCollection(Holders, k)
EndFunction

Function ClearCollection(RefCollectionAlias akCollection, Keyword k)
	If !akCollection
		Return
	EndIf
	Int i = akCollection.GetCount()
	While i > 0
		i -= 1
		ObjectReference r = akCollection.GetAt(i)
		akCollection.RemoveRef(r)
		Actor a = r as Actor
		If a
			If k
				a.SetLinkedRef(None, k)
			EndIf
			a.EnableAI(True)
			a.SetHeadTracking(True)
			a.EvaluatePackage()
		EndIf
	EndWhile
EndFunction
