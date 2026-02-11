# SpellGems

SpellGems lets you store a spell inside a soul gem and cast it later with a hotkey. 
It turns spare gems into quick, limited‑use spells and adds special effects for Azura’s Star and Black Soul Gems/Black Star.

## What it does
- Store the spell in your right hand into a selected soul gem.
- Cast stored spells instantly from hotkey slots.
- Spells can be casted while weapons / spells are equipped in either hand.
- Track uses, cooldowns, and fragments by gem tier.
- Optional bonuses for Black Soul Gems and Azura’s Star.
- Soul gems are consumed when storing spells, but reusable stars can be overwritten with new spells without consuming them.
- Used gems will break down into fragments based on their tier, which can be used for crafting.
- Completely modular and customizable using the UI menu.

## How to use
1. Equip a spell in your right hand.
2. Open your inventory and hover over a soul gem.
3. Press the **Store Spell** key to save the spell into the gem.
4. Use the **Activate Gem** hotkeys to cast stored spells.

## Special gems
- **Azura’s Star** and **Black Star** can store any spell tier without needing a soul.
- They remain reusable and can still act like normal soul gems.
- Optional: Azura’s Star can grant a small damage/effectiveness boost.
- Optional: Black Soul Gems can grant stronger bonuses with a small health cost.

## Settings
Customize keys, cooldowns, uses, and bonuses in the settings UI menu or config file. (UI Recommended)

## Requirements
- Skyrim Special Edition (or Anniversary Edition)
- Address Library for SKSE (1.5.97/1.6.1170)
- SKSEMenuFramework
- SKSE (2.0.20/2.2.6)

## Notes
- Regular soul gems are converted into stored‑spell gems when used.
- Reusable stars keep their original item and overwrite the stored spell.

## License
This mod is released under the MIT License. See LICENSE for details.

## Credits
- The SKSE team for the tools and libraries that make this mod possible.
- CharmedBaryon for CommonLibSSE-NG from which this mod is built on.
- Supertron for the tool used to compile this mod. (ClibDT)
- SkyrimThiago for SKSE Menu Framework, which provides the UI components.
