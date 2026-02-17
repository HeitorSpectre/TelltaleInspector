# The Telltale Inspector
This is a modding application which aims to allow modders to mod newer games made by the Telltale Tool (Telltale Games).
For developers wishing to know more about internally how it works, please scroll to the bottom.

<strong>PLEASE DONATE. I PUT LOTS OF TIME INTO THESE TOOLS! [Donation Page](https://gofundme.com/f/lucas-saragosa)</strong>

--------------
# Fork Info
This is a version of Telltale Inspector fully focused on prop editing, including bug fixes related to text editing inside prop files, as well as TXT extraction and reinsertion directly into the prop file.

It allows you to extract all text from a prop file into a clean, fully readable TXT format, making editing much easier and more organized. After editing, you can correctly reinsert the TXT back into the prop file without breaking formatting or structure.

The tool also includes an encoding conversion system, allowing you to switch between ANSI and UTF-8 when necessary. This ensures compatibility depending on the specific game, giving you full control over the file’s encoding format.

<img width="854" height="501" alt="image" src="https://github.com/user-attachments/assets/55cf9e47-3d4f-4bef-918e-b44562b1cb65" />

-----------------

## Credit
This tool was made possible by years of reverse engineering by solely me. Over this time, I have had various help from people from the Telltale Modding discord and more. Below are the people who helped me along the way:

#### David M:
David is such a massive help and has guided me through rendering and the telltale tool from the beginning. He provided me with the WDC .PDB which without would have meant none of this is possible. [David's Github](https://github.com/frostbone25)
#### Aluigi Auriemma:
The original creator of ttarchext, would not have initially been able to read archives without help from him before the PDB. [Personal Website](https://aluigi.altervista.org/)
#### RandomTalkingBush:
He spent years on the 3DSMax D3DMesh importer which I still find myself sometimes referencing to help make difference between different D3DMesh file versions as I can only know 100% about the format in the Walking Dead Collection version of the engine. [RTB'S Github](https://github.com/RandomTBush)
#### Simon Pinfold
Very useful python script which provided information on the proprietary fmod sound bank version 5 format. (FSB5). [Simon's Github](https://github.com/HearthSim/python-fsb5)
#### Aizzble ('arizzble')
Continuous support and testing help from aabi, and my partner in building all of [Minecraft Story Mode in Minecraft](https://www.planetminecraft.com/project/mcsm-rebuilt-in-minecraft/) by using my tools and code to convert meshes into Minecraft Java.
#### TheCherno
Known for this game engine youtube channel, he also not only taught me C++ very well but also provided the backend GUI framework library <strong>Walnut</strong> which means in one line of code I have a GUI. The GUI library used is the iconic Dear ImGui. [TheCherno Youtube](https://www.youtube.com/channel/UCQ-W1KE9EYfdxhL6S4twUNw)
#### Other Credit
Other authors code has been used such as the C++ file dialog helper library NFD and more.

#### Fork Credits
BlueSkyWestSide for fix compilation.
 