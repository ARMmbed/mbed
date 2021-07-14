# mbed-os-env docker management


### Overview and background

docker image mbed-os-env bundles all the necessary tools to provide a minimal environment to build and test mbed-os applications. This docker image shall be used in Continuous Integration pipelines or in other use cases where mbed-os tools and dependencies are required. This document explains versioning strategy of the docker image and github action workflows that creates these docker images.


### Types of docker images

**Production docker image**
These are docker images compatible with a released version of mbed-os. For example when `mbed-os-6.14.0` is released, a docker image with tag `mbed-os-6.14-latest` is available.

**Development docker image**
These are docker images compatible with a `HEAD` of the mbed-os master branch. These are tagged with `master-latest`. 


### Type of docker image updates

There are two type of docker image updates. 

**Active Updates** 
Typically these type of changes that involve modifications in Dockerfile or changes in the requirements.txt. Examples: mbed-os adds support for new compiler version and hence needs Dockerfile update.

**Passive updates** 
These are scheduled updates on existing mbed-os-env images to provide security and bug fixes of dependent packages. In updates like this, there will be no change in Dockerfile but rebuilding the Dockerfile provides a new docker image with updated tools and packages. 

### Type of docker tags
For the purpose of mbed-os-env docker management we will need to apply a few docker tags on the image. 

**latest tags**
These tags have the format `<name>-latest`, for example `mbed-os-6-latest`. The `-latest` at the end indicates that the same tag may be reapplied to another image, when a docker image is updated. This obviously means, a user pulling the image with same tag name may get different images at different point of time. 

**fixed tags**
using latest tags, user can retrieve an updated version of docker image. Troubleshooting could become tricky if docker images are updated and new images are retrieved in the middle of troubleshooting session. Also, some user could decide not to get updated version and would want to stick to a fixed version.

For reasons stated above, all the docker images are also applied a fixed tag. These tags are only applied to one image and never reused. Tags are of the format `mbed-os-<version>-<date>`, example `mbed-os-6.14.0-2021.06.19T04.43.51`, `main-2021.06.19T04.43.51` etc

### Dockerfile versioning
mbed-os-env image is created with dockerfile stored in this repository itself. This provides easy versioning of Dockerfile as it will follows the same versioning strategy of mbed-os. 

### Docker image versioning

The picture below illustrates a typical situation where mbed-os accepts changes to master branch on daily basis and makes releases on regular basis.

![Docker Versioning](./diagrams/docker-versioning.png)

**On Day-X** There are some changes for [active updates](#Type-of-docker-image-updates) (for example dockerfile has been changed), so these are the docker images created or updated

* mbed-os-env:master-latest 
* mbed-os-env:<master-day-x> - This is a fixed tag

:information_source: master-latest tag is checked for [passive updates](#Type-of-docker-image-updates) as scheduled in the workflow.

**On Day-X+1** Though there are commits to mbed-os source repository. These do not involve changes to Dockerfile or dependencies like requirements.txt. Hence, no docker image is created at the time of merging the commit. 

**On Day-X+2** A new mbed-os release `mbed-os-6.14.0` is created. This creates an image with following docker tag.

* mbed-os-env:mbed-os-6-latest - This docker tag could be used to get a compatible image for latest mbed-os-6 release.
* mbed-os-env:mbed-os-6.14-latest - This docker tag  could be used to work with updated mbed-os-6.14 release. This image is passively updated till next mbed-os release on the branch (typically till mbed-os-6.15.0)
* mbed-os-env:mbed-os-6.14-day-x - A fixed docker tag 

:information_source: mbed-os-6.14-latest, and mbed-os-6-latest will be passively updated as scheduled in th workflow.

**On Day X+10** Another new release mbed-os-6.15.0 is made. This creates an image with following docker tag.

* mbed-os-6-latest
* mbed-os-6.15-latest
* mbed-os-6.15-day-x

:information_source: From this point, mbed-os-6.14-latest will no longer be passively updated. From this point, only mbed-os-6.15-latest is selected for passive update. Only last release version is passively updated on a branch. 

## GitHub Action Workflows

### Goals of docker management workflows

* Distribution of docker image that is compatible with released version of mbed-os. 
* Providing docker image that is compatible with HEAD of "main" branch. 
* Keeping the released and development docker image up to date.

There are 4 main workflows

**PR check**
The purpose of this workflow is to make sure build, and test of mbed-os-env docker image works as expected. Hence this workflow is triggered when mbed-os PR is created with changes in Dockerfile, test files, or workflow files itself has some modifications. Since most of the mbed-os PR doesn't contain docker image related changes, this workflow is not expected to be triggered often.

**Publish docker image for the HEAD of mbed-os branch**
The purpose of this workflow is to update development docker image either when there is an active update or at nightly for passive update.

:bulb: This workflow can also be triggered manually to update the development image (during the day if needed for example)

**Production Docker image Creation/Update**
This workflow will create a new docker image with versioning strategy describe above. Also, triggered nightly for passive update.

:bulb: This workflow can also be triggered manually to update an old version of released docker image. For example, this workflow could be manually triggered to update mbed-os-6.14-latest after mbed-os-6.15.0 is released

**Prune docker image**
docker image in temporary area is pruned by this workflow. Number of dates since updated is used as criteria for pruning.

### Pipeline

docker image management follows typically CI pipeline of build, test, release follows.

There are some details worth mentioning though.

**Build**

docker buildx command is used for creating multi architecture docker image. To build docker image using buildx, one needs to push the image to a remote repository while building it. Since, we need to "test" before release, the resulting images are pushed to a temporary docker repository just after building.

github container registry doesn't implement yet all the docker manifest APIs. Hence, a few features like deleting tag from an image is not available yet. Once these are implemented, we may remove temporary repository and add temporary tags to the image and delete the temporary tags after workflow.

**Test**
Once temporary images are built, we need to verify whether image built is same as the image already available as our intended docker image version. This is achieved by comparing the docker digest of two images. Obviously, if digest is same, there is no need to make another release as docker image available as released version is already up to date.

If the digest is different, (means, new version of docker image is built), test.sh script takes care of all the testing.

**Deploy**
In Deploy job, depending on the result of test job, temporary images are moved to production repository.

### Docker repository
github provides free docker image storage for public repositories in github packages. The workflows make use of {{ secrets.GITHUB_TOKEN }}  https://docs.github.com/en/actions/reference/authentication-in-a-workflow 

The packages are directly visible in mbed-os repository.

For deleting images from temporary repository, a new token GITHUB_DELETE_IMAGE_TOKEN needs to be added with package delete permissions.

There are two repositories being created or used for managing docker image 

**mbed-os-env** This is the public repositories user can pull the images from. 

**mbed-os-env-tmp** This is a private repositories created for temporary management of docker images while docker images are being tested. This is a staging area. Unfortunately, github container registry do no support renaming docker tags, hence the need for a temporary repository.

### Workflow for forks

As there is a scheduled trigger of workflow for development and release images, which inturn creates and updated docker images in github container registries, workflows are enabled only when "ARMMbed" repository owner criteria is met. For development, one can change this, make necessary development and put this back to "ARMMbed" when creating pull request to "ARMMbed/mbed-os".

### Usage in example application
![example app](./diagrams/example-app-usage.png)
Example application repository mainly has three branches

* master - This branch points to the latest last release version of mbed-os. 
* development - This branch points to the master branch of mbed-os.
* release_candidate - This branch points to the release_candidate tag of mbed-os.

*Release workflow of mbed-os*
* A rc tag is applied to mbed-os (for example mbed-os-6.16.0-rc). 
* `development` branch of mbed-os is merged to `release_candidate`. Also, applies rc tag to mbed-os.lib
* After RC tests are done, mbed-os release tag is applied (for example mbed-os-6.16.0-rc)
* `release_candidate` branch is merged to master and release tag is applied `mbed-os-6.16.0`

For the above branches `mbed-os-env:master-latest` can be used as at the time of `release_candidate`(and `release tag` creation), example application will be pointing to master version of mbed-os. 

