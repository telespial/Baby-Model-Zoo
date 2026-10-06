# Baby Model Zoo

## Small, Specialized EdgeAI Models for Embedded Systems

Baby Model Zoo is a collection of small model projects for resource-constrained embedded systems. The repository currently contains the Penguin temperature anomaly detector, documented in [`penguin-temperature-anomaly-detector`](./models/penguin-temperature-anomaly-detector/README.md).

Penguin is an implemented host-and-firmware demonstration: its source contains two small temperature models, host tests, an NXP FRDM-MCXC162 firmware target, and a browser dashboard. The project README distinguishes host validation from hardware validation and documents the experimental, non-safety-critical scope.

Model capabilities, inputs, outputs, training workflow, and deployment requirements are documented per model. This repository does not claim support for a particular MCU runtime unless the model documentation and source establish it.

## Related projects

- [EmbeddedX specifications](https://github.com/telespial/EmbeddedX-Specs)
- [NXP EdgeAI demos](https://github.com/telespial/NXP-EdgeAI-Demos)
- [Infineon EdgeAI demos](https://github.com/telespial/Infineon-EdgeAI-Demos)
- [STMicroelectronics EdgeAI demos](https://github.com/telespial/STMicro-EdgeAI-Demos)

See [LICENSE](./LICENSE) for license terms.
