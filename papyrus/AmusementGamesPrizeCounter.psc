Scriptname AmusementGamesPrizeCounter extends ObjectReference
{The Prize Counter's screen, as the Nuka-Cade's prize terminal's: its tokens (tickets redeemed and unspent, redeemed
all told, the tickets on you, your best scores: globals of AmusementGames.esl) put on it, and you and it put in AmusementGamesQuest's
aliases (Player stores your name, PrizeCounter uses it) for its welcome. Done as it loads, and again as you use it.}

Event OnInit()
	Ready()
EndEvent

Event OnLoad()
	Ready()
EndEvent

Event OnActivate(ObjectReference akActionRef)
	If akActionRef == Game.GetPlayer()
		Ready()
	EndIf
EndEvent

Function Ready()
	AddTextReplacementData("CurrentTickets", Glob(0x83E))
	AddTextReplacementData("TotalRedeemed", Glob(0x83F))
	AddTextReplacementData("CarriedTickets", Glob(0x845))
	AddTextReplacementData("HighScoreAtomicRollers", Glob(0x840))
	AddTextReplacementData("HighScoreHoopShot", Glob(0x841))
	AddTextReplacementData("HighScoreWhacACommie", Glob(0x842))
	AddTextReplacementData("HighScoreNukaZapperRace", Glob(0x843))
	AddTextReplacementData("HighScoreBanditRoundup", Glob(0x844))
	Quest q = Game.GetFormFromFile(0x80F, "AmusementGames.esl") as Quest
	If q
		ReferenceAlias you = q.GetAlias(4) as ReferenceAlias
		ReferenceAlias counter = q.GetAlias(5) as ReferenceAlias
		If you && you.GetReference() != Game.GetPlayer()
			you.ForceRefTo(Game.GetPlayer())
		EndIf
		If counter && counter.GetReference() != Self
			counter.ForceRefTo(Self)
		EndIf
	EndIf
	; (the tickets on you now: the plugin sets it as you activate; this in case it couldn't)
	GlobalVariable held = Glob(0x845)
	MiscObject ticket = Game.GetFormFromFile(0x019988, "DLCNukaWorld.esm") as MiscObject
	If held && ticket
		held.SetValue(Game.GetPlayer().GetItemCount(ticket))
	EndIf
EndFunction

GlobalVariable Function Glob(Int aiID) Global
	Return Game.GetFormFromFile(aiID, "AmusementGames.esl") as GlobalVariable
EndFunction
