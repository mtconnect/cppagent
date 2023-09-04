# MTConnect Demo - Docker

There are three ways to run the MTConnect demo:
* All processes in a single Ubuntu container
* All processes in a single Alpine container
* Composed Alpine adapter containers and agent Alpine containers

## Building the images

### Building the Demo for Ubunto

From the `demo` directory build the docker image:

        docker build -t mtconnect-demo:latest -f Dockerfile ..
        
### Building the Demo for Alpine

From the `demo` directory build the docker image:

        docker build -t mtconnect-demo:latest -f Dockerfile.alpine ..
        
### Building the Demo for with Docker Compose

From the `demo/compose` directory

        docker compose build

## Running the demo mapping the host's port `5001` to port `5000`

        docker run -it --rm -p 5001:5000 mtconnect-demo

Then open a browser to http://localhost:5001/

* For the mazak: http://localhost:5001/twin/index.html?device=Mazak
* For the Okuma: http://localhost:5001/twin/index.html?device=OKUMA

## Running the docker compose version

       docker compose up

Then open a browser to http://localhost:5001/

* For the mazak: http://localhost:5001/twin/index.html?device=Mazak
* For the Okuma: http://localhost:5001/twin/index.html?device=OKUMA

