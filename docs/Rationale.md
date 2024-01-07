# Rationale

[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[so_5_extra]: https://stiffstream.com/en/products/so_5_extra.html
[so_5_infra]: https://stiffstream.com/en/docs/sobjectizer/so_5-5/namespaceso__5_1_1env__infrastructures.html
[so_5_loops]: https://github.com/eao197/so-5-5/issues/25
[caf]: https://actor-framework.org/
[qpcpp]: https://www.state-machine.com/qpcpp/
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio
[erlang-sup]: https://learnyousomeerlang.com/supervisors#supervisor-concepts
[bas-suprevisors]: https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors
[rotor]: https://github.com/basiliscos/cpp-rotor

There are few main actor frameworks in C++ world: [sobjectizer], [c++ actor framework][caf]
and [QP/C++][qpcpp]. There is major a issue with them, as they offer some actor runtime, which
has *own* rules for execution, which user have to comply. On the other hand there are a lot of
third-party libraries, which often have other runtime. As the result, the seamless usage of
them together becomes hard or even impossible, or it might come at rather high costs (i.e.
running each environment /runtime  on it's own thread). In the other words they are **intrusive**.

Their intrusiveness makes the frameworks same sort unfriendly for cooperation with other C++
libraries. However, why they are intrusive? To my opinion, this happens because the frameworks
need, first, *timers* and, second, some inter-thread message passing and actor execution
environment. So, for the frameworks own intrusive runtimes were developed.

[rotor] was designed with the opposite approach: let timeouts and inter-thread communications
be handled by external libraries (loops), while [rotor] responsibility is just internal message
delivery and a thin timers abstraction layer; everything beyond that is just messaging rules.  As
the result [rotor] is quite lightweight micro-framework; the main infrastructure units of execution (aka
`supervisors`) are tightly coupled to the concrete event loop. However the last point shouldn't
be a big issue, as soon as the [rotor] *messaging interface* does not depend on underlying
loop. In [rotor] messages can be passed between actors running on different threads or even
actors running on different event loops, as *soon as underlying supervisors/loops support
that*. This makes it possible to scale [rotor] across different event loops within the same
program.

The messaging rules and **asynchronous and composeable lifetime** of actors, makes it possible to
create [erlang-like](https://en.wikipedia.org/wiki/Erlang_(programming_language)#Supervisor_trees)
**hierarchies of supervisors**, i.e. hierarchies of responsibilities. As far as I now neither
[caf] nor [sobjectizer] have something similar. It is better to familiarize yourself
with the original [erlang supervisors concepts][erlang-sup] or from my
[article][bas-suprevisors] explaining problems of supervisors related to C++ world.

Another design feature of [rotor] is it's **testability**: as long as core application
logic is I/O-pure, the whole testing scenario is fully deterministic and without event loop.
Rotor-based applications should have moderate complexity test-environment setup. This can
be achieved via two corner stones: unintrusiveness (feature of [rotor]) and interface weakly
coupling because actors just react on  incoming messages (common feature of all actor frameworks).

As the last point, it should be noted, that [sobjectizer] is not that intrusive as [caf],
because it supports various [infrastructures][so_5_infra] and because there is an additional
external module [so_5_extra], which already has [boost-asio] support. The external loop
integration is still [non-trivial task][so_5_loops] compared to [rotor], and I'm still not
sure whether is is possible to integrate more than one event loop.

[rotor] misses some features of the named frameworks, like messaging overloading control
and networking in the exchange for more *fine-gained* control and *lightweightness*.
