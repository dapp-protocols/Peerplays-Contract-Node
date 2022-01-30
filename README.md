## Peerplays Node

This repository contains a stripped-down Peerplays node design which is based upon dynamic linkage and configurability rather than a static conglomeration of modules. As a result, the node itself should remain quite lean and focused on generality and flexibility, while the advanced functionalities are added by dynamic modules.

PLEASE NOTE that this implementation is extremely early in development and does not yet fulfill the promise of a dynamically configurable and instrumentable blockchain node. It is, however, the hope and intent of the DApp Protocols to grow this implementation to fulfill that promise eventually.

This work is based upon the DApp Protocols' fork of Peerplays available [here](https://github.com/dapp-protocols/peerplays).

#### Architectural Notes
The node is based around the `ContractNode` class, which is responsible for loading the relevant modules and keeping the program alive until the user wishes it to shut down. At present, the `ContractNode` directly and statically instantiates and configures the `P2pHandler` and `ChainHandler` modules, which manage the P2P node and chain database respectively. Eventually, the `ContractNode` class will be abstracted away into generalized infrastructure, and the modules will operate autonomously to carry out the operations of the node.

The node is designed to support the functionality of loading smart contracts into the chain as dynamically linked modules at runtime. This is implemented via the [ContractApi](ContractApi/ContractApi.hpp) interface, which exposes a simple registration function that registers the contract's evaluators and indexes into the chain database at initialization time.
