Scriptname AmusementGamesQuest extends Quest
{Amusement Games: settlers walked to a machine to play it. The Players collection carries AmusementGamesWalkToSpot (walk
to their linked ref, keyword AmusementGamesLinkSpot, and face it), which outranks the sandbox packages settlers usually
have (as Wasteland Wagers walks its bettors and dealers). Actors, refs and keywords come in as form IDs.}

RefCollectionAlias Property Players Auto Const Mandatory

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

; out of Players: their own AI again
Function Leave(Int aiActor, Int aiLink)
	Actor a = Game.GetForm(aiActor) as Actor
	Keyword k = Game.GetForm(aiLink) as Keyword
	If !a
		Return
	EndIf
	Players.RemoveRef(a)
	a.EnableAI(True)
	If k
		a.SetLinkedRef(None, k)
	EndIf
	a.EvaluatePackage()
EndFunction

; everyone out (a save loaded: whoever the plugin had walking or playing is gone from its mind)
Function ClearAll(Int aiLink)
	Keyword k = Game.GetForm(aiLink) as Keyword
	Int i = Players.GetCount()
	While i > 0
		i -= 1
		Actor a = Players.GetAt(i) as Actor
		Players.RemoveRef(Players.GetAt(i))
		If a
			If k
				a.SetLinkedRef(None, k)
			EndIf
			a.EnableAI(True)
			a.EvaluatePackage()
		EndIf
	EndWhile
EndFunction
