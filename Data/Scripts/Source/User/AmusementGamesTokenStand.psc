Scriptname AmusementGamesTokenStand extends ObjectReference
{The Token Stand: caps in, Nuka-Cade tokens out, 5 at a press of F. The machines take the tokens.}

MiscObject Property Caps Auto Const Mandatory
MiscObject Property Token Auto Const Mandatory
GlobalVariable Property TokenPrice Auto Const Mandatory
{caps a token}
Message Property NotEnoughCapsMessage Auto Const Mandatory
Sound Property DispenseSound Auto Const Mandatory

Int Property TokensEach = 5 Auto Const
{tokens a press}

Event OnActivate(ObjectReference akActionRef)
	Actor player = Game.GetPlayer()
	If akActionRef != player
		Return
	EndIf
	Int price = TokenPrice.GetValueInt()
	If price < 1
		price = 1
	EndIf
	Int cost = TokensEach * price
	Int haveCaps = player.GetItemCount(Caps)
	If haveCaps < cost
		NotEnoughCapsMessage.Show(cost, haveCaps)
		Return
	EndIf
	player.RemoveItem(Caps, cost)
	DispenseSound.Play(Self)
	player.AddItem(Token, TokensEach)
EndEvent
