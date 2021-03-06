### JuPedSim
------------
Simulating pedestrians with state of the art models.
The Jülich Pedestrian Simulator ([JuPedSim](http://www.jupedsim.org)) consists of three modules which are loosely
coupled and can be used independently at the moment. These are:

1. `JPScore`: the core module computing the trajectories. At the moment the [Generalized Centrifugal Force Model](http://arxiv.org/abs/1008.4297) is implemented. Another (yet unpublished) model is implemented.
2. `JPSvis`: a tool for visualising the input (geometry) and output (trajectories) data.
3. `JPSreport`: a tool for analysing the trajectories and validating the
model. It implements a couple of measurement methods including the [Voronoi-method](http://dx.doi.org/10.1016/j.physa.2009.12.015) for calculating the density.


### Repository
-------------
The code is *actively developed* in a [GitLab repository](https://cst.version.fz-juelich.de/public/projects). Please consider cloning the code there.  To be **notified** about releases and updates please follow us [@JuPedSim](https://twitter.com/JuPedSim).


### Frequently Asked Questions
-------------------------------
1. What is the official page of `JuPedSim`?
  * www.jupedsim.org and the contact is `info at jupedsim.org`. You will find more information on the working group and other tools and experimental pedestrians data we have been collecting over the years.


2. Where is the official repository ?
 * `JuPedSim` is developed at the [Forschungszentrum Jülich](http://www.fz-juelich.de) in Germany and the bleeding edge code is in their intern git [repository](http://cst.version.fz-juelich.de). At the moment only specific tags are pushed to Github.

3. Is there a manual ?
 * Of course, the user's guide is found in the downloaded archive.

4. Are the models validated ?
 * We are actually setting up verification and validation tests. Some verification tests are included in this version but most of them will be available with the next version.

5. How can I contribute to `JuPedSim`?
 * Testing and reporting bugs will be great. If you want to contribute actively to the code, by implementing new models and/or features, you are welcome to do so. Please contact uns per mail at 
 `info at jupedsim.org` so that we can grant you access to the repositories.
 
6. And the License?
  * JuPedSim itself is released unger LGPL and the included librairies under their respective licenses of course.
 