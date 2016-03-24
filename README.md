### mezzo main repository
#Mezzo mesoscopic traffic simulation and BusMezzo mesoscopic transit simulation models

#structure
There are two proper branches at the moment: master and busmezzo_test
In master all the deployable code (stable) resides.
In busmezzo_test the new features are being developed and tested. Whenever something is stable enough to be put in master, you 
create a pull request from busmezzo_test to master and I will test and then merge (if it is indeed stable and does not break anything)


#old SVN structure
In each branch (master and busmezzo_test) you see Directories called 'branches' and 'trunk'.

these are left over from the previous SVN source control, where there was a trunk for the main mezzo development and branches for some new 
developments such as busmezzo. When I used github SVN import, it created one branch (master) with the old SVN branches and trunk as directories. 
Until we have cleaned up all irrelevant old branches and merged busmezzo and plain mezzo code we will have to live with this structure (and 
unnecessary files).

In principle everyone should clone only busmezzo_test, and commit and push there. Moreover, within the busmezzo_test repository, you should only 
work in branches/busmezzo so that the other code (in trunk directory) stays unchanged.

