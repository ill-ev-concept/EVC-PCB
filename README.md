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

```git clone [INSERT]```

If using Desktop, you can click the dropdown menu next to current repository, then "add", and finally "clone" where we can input the repo's url. 

# Setting Paths

After cloning, open KiCAD and find the "Preferences" dropdown and select "Configure Paths".
We are going to be adding three new paths. Create three new variables, ```EVC_SYMBOL_DIR```, ```EVC_FOOTPRINT_DIR```, and ```EVC_3DMODEL_DIR```. Link these to the location of the ```symbols```, ```footprints```, and ```3d_models``` folders contained with ```libs``` folder.

When designing, we will need KiCAD to know to look at a given library. We can do this by going to "Preferences" and finding either "Manage Symbol Libraries" or "Manage Footpring Libraries". Opening either brings up a window where we can select "Project Specific Libraries" where we can add paths. Click the folder icon and add the wanted library. If you set the path up correctly, the library path will contain  ```${EVC_SYMBOL_DIR}```, ```${EVC_FOOTPRINT_DIR}```, or ```${EVC_3DMODEL_DIR}```. 


# Contributing 

In order to begin designing, create a branch for your given board. This can be done through either the "current branch" dropdown in Desktop or using the command ```git checkout -b [branch name]```. You can swap branches using the same dropdown or either ```git checkout [branch name]```. 

This will be the branch where you will be doing your design work. Create a new KiCAD project and save the location into the boards folder of this repo. Everything onward will be traditional git. Use ```git add``` or check off the needed boxes on Desktop to add files on commit. ```git commit -m "[MESSAGE HERE]"``` will commit your changes. Make sure to write a meaningful commit message in order for good documentation. An example is shown below.

```git commit -m "Finished designing LDO and CANBus connectors, need to add MCU pinout and decoupling caps"```

Input both your contributions and the next required steps in order to give reminders to both yourself and your teammates. 

Once a board is finished or you want it reviewed, it must be submitted via a pull request. This can be done using ```git push -u origin [branch name]``` or by clicking the branch dropdown in Desktop and selecting the "create pull request" option. This will open up a Github tab on your browser where you can edit your pull request. Give it a good title and description so the design choices are clear and understandable. A title will also help the leads parse what you guys want done. 

After the PR is submitted, we will decide on whether the board can be pushed to main or not. If it is not pushed, we will submit comments on what needs to be reviewed and edited. You can view open PRs and their comments in Github.

Apart from designing in KiCAD, it is important to be updating the libs folder when any new schematic, footprint, or model is added. First make sure the IC you want to add isn't currently within our library. Add the path to the specific library which is detailed more above. If the IC is not located in it's library, open Symbol/Footprint editor and select the library you want to add to. I recommended clicking the "View" dropdown, selecting the "Panels" option and adding the "Library Tree" option in order to easily see the connected libraries. From this, import the downloaded model and save the file. From there, push the updated library files and create a pull request. The leads will then check your work and merge it into main so everyone can use the IC.

When making pull requests for ICs, make sure to push them individually rather than with changes to other files. This will make it more clear for reviewers to understand that library files are being edited in addition to making sure that the change will be pushed to main. If you push with changes to your board and the pull request is not granted, people will not have access to the IC you have imported. 

Because the libs folder will be constantly updated, its also important to keep your branch up to date with new changes. We can layer on changes from main into our branch using a few different methods but the main one we will be using is merge. Run the command ```git fetch origin``` and then ```git merge origin``` in order to update changes. In Desktop, you can selete "Update from main" from the "Branch" dropdown. This should not change any local files that is stored and will update the libs folder to have the newest selection of ICs. I recommened doing this every time you work on your boards. It will also make sure a merge conflict won't be created, in which the same file from two different pull requests is edited, causing a lot of hassle for the writers of this repo. 

A general outline of what the workflow may look like is shown below:

```git checkout [branch name]``` -> Run this to swap to the branch you want to work on

```git fetch origin``` -> Run this to pull updates from main. Important for having updated libs

```git merge origin/main``` -> Merges the changes from the above command

```git add [file name]``` -> Add files for commit. When updating library files, make sure to add them in indivual PRs

```git commit -m "[message here]"``` -> Commits your changes to be pushed into your branch

```git push -u origin [branch name]``` -> Run this command the first time you make the branch. It will link your branch to Github. 

```git push``` -> Pushes your changes. After, you can go to Github and create a pull request for the changes to be pushed into main. **Don't create a PR for every change you do.** Only create pull requests **IF** you are editing the libs folder **OR** you are submitting for board revision **OR** it is the final revision of the board.

