FROM alpine:latest

RUN apk add python3 py3-pip
RUN pip3 install Flask cachelib

# By default run on port 5000, because README.md currently mentions it
ARG PORT=5000
EXPOSE $PORT

COPY . BuildYourOwnLisp
WORKDIR BuildYourOwnLisp
# Make the containerized Flask server visible from the host machine
RUN sed --in-place s'/\(app.run(.*\))/\1, host="0.0.0.0")/' lispy.py
# Set PORT environment variable to ARG 'PORT' for lispy.py
ENV PORT=$PORT
CMD python3 lispy.py
