Scriptname AmusementGamesPrizeCounter extends ObjectReference
{The Prize Counter's screen, as the Nuka-Cade's prize terminal's: its tokens (tickets redeemed and unspent, redeemed
all told, the tickets on you, your best scores: globals of AmusementGames.esl) put on it, and you and it put in AmusementGamesQuest's
aliases (AmusementGamesTextQuest's: Player stores your name, PrizeCounter uses it) for its welcome. Done as it loads, and again as you use it.}

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
	; (the high scores: Top<Game><NN>, a score each, 0x848 on, ten a game in AmusementGames' order)
	String[] games = new String[5]
	games[0] = "AtomicRollers"
	games[1] = "HoopShot"
	games[2] = "WhacACommie"
	games[3] = "NukaZapperRace"
	games[4] = "BanditRoundup"
	Int g = 0
	While g < 5
		Int place = 1
		While place <= 10
			String nn = place as String
			If place < 10
				nn = "0" + place
			EndIf
			AddTextReplacementData("Top" + games[g] + nn, Glob(0x848 + g * 10 + place - 1))
			place += 1
		EndWhile
		g += 1
	EndWhile
	Quest q = Game.GetFormFromFile(0x8B2, "AmusementGames.esl") as Quest   ; (AmusementGamesTextQuest)
	If q
		ReferenceAlias you = q.GetAlias(0) as ReferenceAlias
		ReferenceAlias counter = q.GetAlias(1) as ReferenceAlias
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
