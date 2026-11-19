Scriptname AmusementGamesTokenStand extends ObjectReference
{The Token Stand: caps in, Nuka-Cade tokens out. The machines take the tokens.}

MiscObject Property Caps Auto Const Mandatory
MiscObject Property Token Auto Const Mandatory
GlobalVariable Property TokenPrice Auto Const Mandatory
{caps a token}
Message Property BuyMessage Auto Const Mandatory
{the price, your caps and tokens; buttons 1, 5, 10, 25 tokens, Cancel}
Message Property NotEnoughCapsMessage Auto Const Mandatory
Sound Property DispenseSound Auto Const Mandatory

Event OnActivate(ObjectReference akActionRef)
	Actor player = Game.GetPlayer()
	If akActionRef != player
		Return
	EndIf
	Int price = TokenPrice.GetValueInt()
	If price < 1
		price = 1
	EndIf
	Int caps = player.GetItemCount(Caps)
	Int choice = BuyMessage.Show(price, caps, player.GetItemCount(Token))
	Int count = 0
	If choice == 0
		count = 1
	ElseIf choice == 1
		count = 5
	ElseIf choice == 2
		count = 10
	ElseIf choice == 3
		count = 25
	EndIf
	If count == 0
		Return
	EndIf
	Int cost = count * price
	If caps < cost
		NotEnoughCapsMessage.Show(cost, caps)
		Return
	EndIf
	player.RemoveItem(Caps, cost)
	DispenseSound.Play(Self)
	player.AddItem(Token, count)
EndEvent
