# Gameplay collision events
Physics reports engine shape identities and physical contact/overlap values. GameplayCollisionRuntime owns actor/team/role bindings and converts these into Body, PushBox, Sensor and HitBox events.

Attack-instance deduplication and damage policy remain above physics. A sensor's presence does not imply hostility or damage.

The runtime retains value-semantic binding information for active pairs so an End can be routed after actor unbinding. It does not look up a destroyed Box2D object. Reset and scene teardown clear bindings and cached routes together.

Begin/Stay/End describe sampled contacts. Sensors are discrete; fast attacks require explicit casts. Physical bullet CCD does not make Sensors continuous.

Listeners may request mutations during dispatch; the current event batch remains stable. Listener objects must outlive that batch.
