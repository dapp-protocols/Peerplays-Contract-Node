# The Infra Library

The Infra Library is a collection of patterns, protocols, and tools used to instrument code so that it can be examined
and manipulated both at compile-time and run-time. The goal is to enable modular software to process its own structure
so as to automate more and more of the work required to develop software, making iterations more and more rapid, and
eventually, opening up new possibilities for software to automatically negotiate protocols and configurations without
requiring manual intervention, while simultaneously making it possible to inspect exactly how the software is built
and configured, whether at compile-time or at run-time.

As its name suggests, Infra (meaning "under" or "below") seeks to provide, not merely utilities and functions helpful
for developing software, but also a powerful infrastructure that underlies the software paradigm itself and opens up
previously unseen possibilities for automation in software development tasks. For example, Infra provides the Modular
framework which allows demarcating the modules of a program, the submodules of those modules, and so on recursively,
allowing code to be written which automatically processes this entire modularization hierarchy, inspecting each module
individually and processing it for arbitrary purposes. Infra also provides the API framework, which uses the Modular
framework to search a modularization tree for modules exposing APIs, and bringing those APIs together into a list in
order to, for example, expose them all together over an RPC interface.

At the heart of Infra is the DMarc pattern, short for demarcation point. As Infra seeks to provide underlying support
infrastructure for the development and construction of a program, the various types and modules of the program need to
demarcate the mechanisms they provide for Infra to process. The DMarc represents the interface between a specific type
in the program and Infra, and contains a compile-time information registry as well as run-time functionality for Infra
to use to handle the type.

The name "demarcation point" is inspired by the old POTS telephony systems, where the telephone service provider ran
telephone cables to customer houses or buildings, which then had cables inside to connect the various phones and other
equipment to the network. At the edge of the customer property was a box, called the demarcation point, or DMarc for
short, which represented the point where ownership and responsibility for the network handed over from the provider to
the customer: everything outside the property up to and including the DMarc was the responsibility of the provider;
everything after the DMarc was the responsibility of the customer. In a similar fashion, the DMarc for a type contains
the interface between a specific type or module which is part of the program, and the Infra infrastructure which can
be used to connect the modules together or facilitate communication between them.

This code is largely my own work; however, parts are based upon, inspired by, or adapted from the work of others.
