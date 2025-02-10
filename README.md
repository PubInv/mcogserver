# mcogserver
A parameter and datalogging server for the NASA Ceramic Oxygen Generator

# A Note On Control

On Geoffs server, this is treated as a service that can be restarted with:

```
> sudo systemctl start mcogs.service
```

Setup

#environmental setup
sudo apt install -y  build-essential git libssl-dev


#install the local mcogserver
git clone https://github.com/PubInv/mcogserver.git
make 
sudo systemctl start mcogs.service
sudo systemctl enable mcogs.service