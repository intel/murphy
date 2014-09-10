Murphy resource policy manager
==============================

What is Murphy?
---------------

Murphy is a centralized resource policy daemon. Murphy assigns system
resources to applications in an event-based and centralized way, meaning
that resources are given to applications that request them in a priority
order.

Murphy works with many different resource domains and is intended to
handle cross-domain resource conflicts. An example of this might be an
interdependency between power management resources, audio resources, and
video resources. If an application requires an instance of all the three
resource types to work properly, taking a single one away from it should
result in the release of the entire set of resources, since the
application anyway requires the whole set to work properly.

Murphy is designed to be extensively scripted and extended. The logic
behind the decisions is encoded in the scipts in form of policy rules.
These rules will be provided by the system intergrator. The extensions
are implemented as Murphy plugins or external domain controllers.

How does Murphy work?
---------------------

Murphy listens to three input event types:

1.  System events
2.  Application requests
3.  User-provided settings

An input event (such as an application requesting permission to play
audio) may change a value in Murphy internal database. This in turn
triggers the decision making mechanism. The decision is made and
communicated to domain controllers, which enforce the resource limits
for different resource domains, and to the applications competing for
the resources. Resource-aware applications are expected to comply with
the decision, but for some domains the decisions can also be enforced.
For the audio domain it might mean stopping or muting the stream that
was not allowed to play.

Why is Murphy needed?
---------------------

The main idea is to move policy responsibilities away from the
applications. Automatic arbitration of the available resources is
important especially in embedded systems with limited user interaction
capabilities.

The applications need to decide by themselves who can access the limited
resources if there is no centralized resource manager. In order to do
this, all applications have to:

*  understand the resource limits in the system (both software and
   hardware)
*  follow other applications' access to the resources
*  define application priorities for resource access; who can access the
   resource is there is a conflict?
*  handle exceptional cases, such as non-resource aware applications
   accessing the limited resources

It is clear that it is extremely difficult for the applications to
cooperate in this way, and implementing and maintaining this support for
every single application running on a system is a huge undertaking.
Changing a policy would require changes to all applications. But if the
applications offload the resource policing responsibilities to a central
resource manager, the applications only need to use a well-defined
resource API to request access to resources and follow the resource
status. In order to do a policy change, the system integrator only needs
to change the policy in resource manager configuration, and the desired
behavior should automatically follow.

Compilation of Murphy
---------------------

Detailed information on building Murphy, the dependencies and options
can be found at the [documentation](https://01.org/murphy/documentation/compiling-and-installing-murphy).

In general, Murphy is an Autotools-based project, so users who have
used Autotools before should be relatively "at home" with the process
of generating the configure script as well as configuring and compiling
Murphy.
