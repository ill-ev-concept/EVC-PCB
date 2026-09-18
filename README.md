# EVC-PCB
Welcome to EV Concept's PCB Repo!

This is where all of our projects are stored. 

* Boards: Contains design files of all current boards being worked on
* Libs: Contains a wide selection of schematics, footprints, and models of components that are being used
* ArchivedBoards: Contains design files of retired boards *???*
* Documentation: Contains documentation of current and retired boards *???*

# Getting Started

Start by having Git and KiCAD 10 downloaded. If you are using a older version of KiCAD, we recommend updating to the newest version. We also recommend installing Github Desktop in order to make the usage a bit cleaner.

Clone this repo. This can be either done through terminal or Github Desktop. Click the green dropdown labeled "<> Code" and select your preference of how to clone the repo. If using terminal, run the command below

```git clone [INSERT ]```

If using Desktop, you can click the dropdown menu next to current repository, then "add", and finally "clone" where we can input the repo's url. 

# Setting Paths

After cloning, open KiCAD and find the "Preferences" dropdown and select "Configure Paths".
We are going to be adding three new paths. Create three new variables, ```EVC_SYMBOL_DIR```, ```EVC_FOOTPRINT_DIR```, and ```EVC_3DMODEL_DIR```. Link these to the location of the ```symbols```, ```footprints```, and ```3d_models``` folders contained with ```lids``` folder.

When designing, we will need KiCAD to know to look at a given library. We can do this by going to "Preferences" and finding either "Manage Symbol Libraries" or "Manage Footpring Libraries". Opening either brings up a window where we can select "Project Specific Libraries" where we can add paths. Click the folder icon and add the wanted library. If you set the path up correctly, the library path will contain  ```${EVC_SYMBOL_DIR}```, ```${EVC_FOOTPRINT_DIR}```, or ```${EVC_3DMODEL_DIR}```. 


# Contributing 

In order to begin designing, create a branch for your given board. 

*//will finish later*


