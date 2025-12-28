.PHONY:
# Make target-specific variables; so as to not pollute the Makefile
byol-docker: PORT := 5000
# prev-byol is deliberately a recursively expanded variable as
# automatic variable $@ is not available outside of the recipes.
# If a docker image with the name 'byol-docker' already exists
# when this Make target is called, it expands to the id of that
# image; else, it expands to an empty string.
byol-docker: prev-byol = $(shell \
                            docker inspect --format '{{ .ID }}' $@ \
                              | sed s'/\(.*:\)\(.*\)/\2/' || echo "")
byol-docker:
# We need to delete (docker rmi) the existing 'byol-docker' image
# before we create a new one or else untagged <none> images start
# piling up. However, we can't `docker rmi` that image before the
# build or else we'd lose the cache and will have  to  start over
# from the base image. Neither can we use `docker rmi --no-prune`
# as it, too, will pile up <none> images since  not  all untagged
# parents of last  image  can be used as cache for the new build.
# (A quick look at the Dockerfile  will  show that  any practical
# rebuild of the image will at least branch off from the Dockerfile
# instruction 'COPY . BuildYourOwnLisp')  Therefore, we use $(eval)
# to create a Make variable, 'last-byol-image', and set it to the
# id of the previous byol docker image, so as to `rmi` it once the
# build is complete.
	$(eval last-byol-image := $(prev-byol))
	docker build --build-arg PORT=$(PORT) --tag $@ .
# Now that the build is complete, delete the previous byol-docker image
# Also, ignore if this recipe's failure (when last-byol-image is "")
	-docker rmi $(last-byol-image) > /dev/null 2>&1
	docker run --rm --publish $(PORT):$(PORT) $@
