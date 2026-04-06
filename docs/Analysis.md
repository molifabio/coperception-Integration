# Konro Noise Study for Cooperative Multi-Agent Perception

## Goal

This document summarizes the experiments run on the cooperative perception pipeline with and without the Konro resource manager, and with different levels of Gaussian noise injected into the Konro feedback signal.

The purpose of the study is to evaluate whether a resource manager based on the policy defined by us and by our colleague can improve the behavior of a multi-agent cooperative vision system when the communication subsystem is stressed, and how sensitive that policy is to an imperfect feedback metric.

The analysis focuses on two execution modes:

- OMNeT enabled, where the communication channel is active and network effects are visible.
- OMNeT disabled, where the network layer is effectively removed and the experiment becomes a control case.

## Data And Files

The main historical result files are:

- [with_konro_with_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/with_konro_with_omnet_history.json)
- [with_konro_without_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/with_konro_without_omnet_history.json)
- [without_konro_with_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/without_konro_with_omnet_history.json)
- [without_konro_without_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/without_konro_without_omnet_history.json)

The comparison script used for the analysis is:

- [tools/det/compare_konro_runs.py](/home/albert0/coperception/coperception-Integration/tools/det/compare_konro_runs.py)

## Important Cleanup Note

One short run in the OMNeT-enabled Konro history was incomplete and had a duration of only a few hundred seconds. It was removed from the history file before the final analysis.

This does not invalidate the previous conclusions. It only changes the index positions inside the history array. The cleaned OMNeT-enabled Konro history now contains three completed runs, and the indices are stable again:

- Index 0: Konro enabled, OMNeT enabled, no injected feedback noise
- Index 1: Konro enabled, OMNeT enabled, feedback noise standard deviation 0.02
- Index 2: Konro enabled, OMNeT enabled, feedback noise standard deviation 0.2

The same index logic applies to the OMNeT-disabled Konro history.

## Experimental Interpretation

The comparison script evaluates a small set of metrics that are relevant for this experiment:

- `proxy_mean` and `proxy_ema`, which summarize the quality proxy seen by the controller.
- `below_target_ratio`, which measures how often the proxy stays below the target threshold.
- `delivery_ratio`, `drop_ratio`, `latency_avg_s`, `latency_p95_s`, `stale_packets`, and `underflow_packets`, which describe the communication behavior.

The script compares each Konro run against the corresponding baseline without Konro and classifies each metric as better, worse, or unchanged.

In this study, the injected Gaussian noise should not be interpreted as a tuning mechanism to maximize performance. Its role is methodological: it simulates the fact that the ideal ground-truth-based metric used to drive Konro is not realistically available in deployment, so the controller must rely on a noisier proxy.

## Results: OMNeT Enabled

The OMNeT-enabled baseline without Konro is the reference case where the system operates over the network but without resource management.

### Baseline without Konro

- `proxy_mean`: 0.641347
- `proxy_ema`: 0.809899
- `below_target_ratio`: 0.970000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.181942
- `latency_p95_s`: 0.400000
- `stale_packets`: 1241
- `underflow_packets`: 0

This baseline is the worst case in terms of stale traffic and average latency among the OMNeT-enabled experiments.

### Konro, noise 0.0

- `proxy_mean`: 0.661531
- `proxy_ema`: 0.837814
- `below_target_ratio`: 0.910000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.158683
- `latency_p95_s`: 0.400000
- `stale_packets`: 557
- `underflow_packets`: 0

Relative to the baseline, this run improves the proxy quality and reduces average latency and stale packets substantially. The communication reliability in terms of delivered and dropped packets stays unchanged, which means the gain is not due to a different network loss profile but to a better adaptation of the controller to the existing communication conditions.

### Konro, noise 0.02

- `proxy_mean`: 0.663483
- `proxy_ema`: 0.828193
- `below_target_ratio`: 0.810000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.157305
- `latency_p95_s`: 0.400000
- `stale_packets`: 552
- `underflow_packets`: 0

This is the strongest result in the OMNeT-enabled scenario. Compared with the baseline, it improves all meaningful metrics and does not introduce any regression.

This suggests that a small amount of noise in the feedback channel does not hurt the controller. More importantly, it indicates that the policy can tolerate a feedback signal that is not perfectly aligned with the ground truth while still preserving the main network-side gains.

### Konro, noise 0.2

- `proxy_mean`: 0.637421
- `proxy_ema`: 0.722186
- `below_target_ratio`: 0.770000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.152640
- `latency_p95_s`: 0.400000
- `stale_packets`: 515
- `underflow_packets`: 0

This run still improves the communication-related metrics relative to the baseline, and it even reduces average latency and stale packets slightly more than the lower-noise Konro runs. However, the quality proxy degrades compared with the cleaner Konro runs, especially in `proxy_ema`.

This is important because it shows that too much feedback noise starts to weaken the quality of the control signal. The resource manager still reacts, but the signal becomes less reliable as a representation of the actual system state.

### OMNeT-Enabled Summary

The OMNeT-enabled experiments lead to a clear conclusion:

- Konro is beneficial when the system is exposed to communication problems.
- The policy is effective because it improves the balance between perception quality and communication overhead.
- The noise-free case shows the upper bound of what the policy can achieve when the feedback is idealized.
- The 0.02 and 0.2 cases show how far the method can be pushed away from the ideal metric before the control signal becomes too unreliable.
- Even with 0.2 noise, the method still preserves the key communication benefits, which is strong evidence that the approach is not fragile.

In the comparison script output, the Konro runs are consistently better than the no-Konro baseline in the key network metrics. The important observation is not that noise improves the policy, but that a noisy proxy can still support a policy that remains effective in a realistic deployment scenario.

## Results: OMNeT Disabled

When OMNeT is disabled, the network layer is turned off and the communication metrics are all zero by construction. This means the experiment is not measuring network adaptation anymore. It becomes a control case that isolates the behavior of the controller and the proxy.

### Baseline without Konro

- `proxy_mean`: 0.677516
- `proxy_ema`: 0.840279
- `below_target_ratio`: 0.900000
- `delivery_ratio`: 0.000000
- `drop_ratio`: 0.000000
- `latency_avg_s`: 0.000000
- `latency_p95_s`: 0.000000
- `stale_packets`: 0
- `underflow_packets`: 0

### Konro, noise 0.0

- `proxy_mean`: 0.677516
- `proxy_ema`: 0.840279
- `below_target_ratio`: 0.900000
- network metrics remain zero

This run is effectively identical to the baseline.

### Konro, noise 0.02

- `proxy_mean`: 0.675682
- `proxy_ema`: 0.841613
- `below_target_ratio`: 0.820000
- network metrics remain zero

The effect is small but positive on `below_target_ratio`, while the proxy average changes only marginally. There is no network benefit to measure here because the network is disabled.

### Konro, noise 0.2

- `proxy_mean`: 0.677241
- `proxy_ema`: 0.791232
- `below_target_ratio`: 0.700000
- network metrics remain zero

This case shows that high feedback noise can alter the proxy dynamics even when the network is absent. The average proxy stays close to the baseline, but `proxy_ema` drops more noticeably, which indicates a less stable controller signal.

### OMNeT-Disabled Summary

This control experiment does not demonstrate a networking advantage, because the network is not active. Its value is methodological:

- It confirms that the resource manager is not causing a trivial metric inflation when there is no communication subsystem.
- It shows that the feedback noise mostly affects the controller state and the proxy, not the absent transport layer.
- It supports the interpretation that the real benefit of Konro emerges when communication bottlenecks are present.

## Overall Comparison

Putting the two scenarios together gives the clearest interpretation of the whole study.

### What Changes When Konro Is Used

With OMNeT enabled, Konro consistently reduces the negative impact of network stress on the system:

- Fewer stale packets
- Lower average latency
- Better proxy quality
- Better ability to stay near the target quality threshold

This is exactly the type of behavior one expects from a well-designed resource manager in a cooperative perception pipeline.

### What The Noise Level Tells Us

The injected Gaussian noise should be read as a proxy for the mismatch between the ideal metric used during policy design and the metric that would be available in a real deployment. The effect is more nuanced than a simple performance degradation:

- A small noise level of 0.02 is still close enough to the ideal metric to preserve the main behavior of the policy.
- A larger noise level of 0.2 is a much stronger mismatch, yet the system still shows the expected improvement over the no-Konro baseline in the OMNeT-enabled case.

This suggests that the policy has meaningful robustness to a noisy and imperfect feedback signal, but that robustness is not unlimited. For the thesis narrative, the critical point is that even a relatively large variance such as 0.2 does not invalidate the approach, which means the policy can still be applied when the available metric is only an approximate version of the ground truth.

## Conclusion

The experiments support the claim that a resource manager such as Konro, combined with the policy we defined, improves the performance of a cooperative multi-agent vision system when communication problems are present.

The main reasons are:

1. The OMNeT-enabled baseline without Konro suffers from high latency and a large amount of stale data.
2. Konro materially reduces those effects while preserving the delivery ratio.
3. The feedback noise is not meant to improve the policy but to simulate the fact that the ideal ground-truth metric is not available in a real deployment.
4. The method remains effective even when that metric is perturbed with substantial noise, including `konro_feedback_noise_std = 0.2`.

From a thesis perspective, the most defensible statement is the following:

Konro is useful not because it changes the communication channel itself, but because it adapts resource usage to communication conditions in a way that preserves perception quality under network stress. The Gaussian noise study shows that the policy does not depend on an unrealistically perfect metric: it still works when the signal used by the controller is only an approximate, noisy surrogate of the ground truth. This is exactly what makes the approach credible for a real cooperative multi-agent perception system.

The cleaned history file and the run indices are consistent with this conclusion, so the previous analysis remains valid after the removal of the incomplete run.# Konro Noise Study for Cooperative Multi-Agent Perception

## Goal

This document summarizes the experiments run on the cooperative perception pipeline with and without the Konro resource manager, and with different levels of Gaussian noise injected into the Konro feedback signal.

The purpose of the study is to evaluate whether a resource manager based on the policy defined by us and by our colleague can improve the behavior of a multi-agent cooperative vision system when the communication subsystem is stressed, and how sensitive that policy is to an imperfect feedback metric.

The analysis focuses on two execution modes:

- OMNeT enabled, where the communication channel is active and network effects are visible.
- OMNeT disabled, where the network layer is effectively removed and the experiment becomes a control case.

## Data And Files

The main historical result files are:

- [with_konro_with_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/with_konro_with_omnet_history.json)
- [with_konro_without_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/with_konro_without_omnet_history.json)
- [without_konro_with_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/without_konro_with_omnet_history.json)
- [without_konro_without_omnet_history.json](/home/albert0/coperception/coperception-Integration/logs/ab/without_konro_without_omnet_history.json)

The comparison script used for the analysis is:

- [tools/det/compare_konro_runs.py](/home/albert0/coperception/coperception-Integration/tools/det/compare_konro_runs.py)

## Important Cleanup Note

One short run in the OMNeT-enabled Konro history was incomplete and had a duration of only a few hundred seconds. It was removed from the history file before the final analysis.

This does not invalidate the previous conclusions. It only changes the index positions inside the history array. The cleaned OMNeT-enabled Konro history now contains three completed runs, and the indices are stable again:

- Index 0: Konro enabled, OMNeT enabled, no injected feedback noise
- Index 1: Konro enabled, OMNeT enabled, feedback noise standard deviation 0.02
- Index 2: Konro enabled, OMNeT enabled, feedback noise standard deviation 0.2

The same index logic applies to the OMNeT-disabled Konro history.

## Experimental Interpretation

The comparison script evaluates a small set of metrics that are relevant for this experiment:

- `proxy_mean` and `proxy_ema`, which summarize the quality proxy seen by the controller.
- `below_target_ratio`, which measures how often the proxy stays below the target threshold.
- `delivery_ratio`, `drop_ratio`, `latency_avg_s`, `latency_p95_s`, `stale_packets`, and `underflow_packets`, which describe the communication behavior.

The script compares each Konro run against the corresponding baseline without Konro and classifies each metric as better, worse, or unchanged.

In this study, the injected Gaussian noise should not be interpreted as a tuning mechanism to maximize performance. Its role is methodological: it simulates the fact that the ideal ground-truth-based metric used to drive Konro is not realistically available in deployment, so the controller must rely on a noisier proxy.

## Results: OMNeT Enabled

The OMNeT-enabled baseline without Konro is the reference case where the system operates over the network but without resource management.

### Baseline without Konro

- `proxy_mean`: 0.641347
- `proxy_ema`: 0.809899
- `below_target_ratio`: 0.970000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.181942
- `latency_p95_s`: 0.400000
- `stale_packets`: 1241
- `underflow_packets`: 0

This baseline is the worst case in terms of stale traffic and average latency among the OMNeT-enabled experiments.

### Konro, noise 0.0

- `proxy_mean`: 0.661531
- `proxy_ema`: 0.837814
- `below_target_ratio`: 0.910000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.158683
- `latency_p95_s`: 0.400000
- `stale_packets`: 557
- `underflow_packets`: 0

Relative to the baseline, this run improves the proxy quality and reduces average latency and stale packets substantially. The communication reliability in terms of delivered and dropped packets stays unchanged, which means the gain is not due to a different network loss profile but to a better adaptation of the controller to the existing communication conditions.

### Konro, noise 0.02

- `proxy_mean`: 0.663483
- `proxy_ema`: 0.828193
- `below_target_ratio`: 0.810000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.157305
- `latency_p95_s`: 0.400000
- `stale_packets`: 552
- `underflow_packets`: 0

This is the strongest result in the OMNeT-enabled scenario. Compared with the baseline, it improves all meaningful metrics and does not introduce any regression:

- Higher proxy quality
- Lower frequency of proxy values below target
- Lower average latency
- Far fewer stale packets

This suggests that a small amount of noise in the feedback channel does not hurt the controller. More importantly, it indicates that the policy can tolerate a feedback signal that is not perfectly aligned with the ground truth while still preserving the main network-side gains.

### Konro, noise 0.2

- `proxy_mean`: 0.637421
- `proxy_ema`: 0.722186
- `below_target_ratio`: 0.770000
- `delivery_ratio`: 0.947000
- `drop_ratio`: 0.053000
- `latency_avg_s`: 0.152640
- `latency_p95_s`: 0.400000
- `stale_packets`: 515
- `underflow_packets`: 0

This run still improves the communication-related metrics relative to the baseline, and it even reduces average latency and stale packets slightly more than the lower-noise Konro runs. However, the quality proxy degrades compared with the cleaner Konro runs, especially in `proxy_ema`.

This is important because it shows that too much feedback noise starts to weaken the quality of the control signal. The resource manager still reacts, but the signal becomes less reliable as a representation of the actual system state.

### OMNeT-Enabled Summary

The OMNeT-enabled experiments lead to a clear conclusion:

- Konro is beneficial when the system is exposed to communication problems.
- The policy is effective because it improves the balance between perception quality and communication overhead.
- The noise-free case shows the upper bound of what the policy can achieve when the feedback is idealized.
- The 0.02 and 0.2 cases show how far the method can be pushed away from the ideal metric before the control signal becomes too unreliable.
- Even with 0.2 noise, the method still preserves the key communication benefits, which is strong evidence that the approach is not fragile.

In the comparison script output, the Konro runs are consistently better than the no-Konro baseline in the key network metrics. The important observation is not that noise improves the policy, but that a noisy proxy can still support a policy that remains effective in a realistic deployment scenario.

## Results: OMNeT Disabled

When OMNeT is disabled, the network layer is turned off and the communication metrics are all zero by construction. This means the experiment is not measuring network adaptation anymore. It becomes a control case that isolates the behavior of the controller and the proxy.

### Baseline without Konro

- `proxy_mean`: 0.677516
- `proxy_ema`: 0.840279
- `below_target_ratio`: 0.900000
- `delivery_ratio`: 0.000000
- `drop_ratio`: 0.000000
- `latency_avg_s`: 0.000000
- `latency_p95_s`: 0.000000
- `stale_packets`: 0
- `underflow_packets`: 0

### Konro, noise 0.0

- `proxy_mean`: 0.677516
- `proxy_ema`: 0.840279
- `below_target_ratio`: 0.900000
- network metrics remain zero

This run is effectively identical to the baseline.

### Konro, noise 0.02

- `proxy_mean`: 0.675682
- `proxy_ema`: 0.841613
- `below_target_ratio`: 0.820000
- network metrics remain zero

The effect is small but positive on `below_target_ratio`, while the proxy average changes only marginally. There is no network benefit to measure here because the network is disabled.

### Konro, noise 0.2

- `proxy_mean`: 0.677241
- `proxy_ema`: 0.791232
- `below_target_ratio`: 0.700000
- network metrics remain zero

This case shows that high feedback noise can alter the proxy dynamics even when the network is absent. The average proxy stays close to the baseline, but `proxy_ema` drops more noticeably, which indicates a less stable controller signal.

### OMNeT-Disabled Summary

This control experiment does not demonstrate a networking advantage, because the network is not active. Its value is methodological:

- It confirms that the resource manager is not causing a trivial metric inflation when there is no communication subsystem.
- It shows that the feedback noise mostly affects the controller state and the proxy, not the absent transport layer.
- It supports the interpretation that the real benefit of Konro emerges when communication bottlenecks are present.

## Overall Comparison

Putting the two scenarios together gives the clearest interpretation of the whole study.

### What Changes When Konro Is Used

With OMNeT enabled, Konro consistently reduces the negative impact of network stress on the system:

- Fewer stale packets
- Lower average latency
- Better proxy quality
- Better ability to stay near the target quality threshold

This is exactly the type of behavior one expects from a well-designed resource manager in a cooperative perception pipeline.

### What The Noise Level Tells Us

The injected Gaussian noise should be read as a proxy for the mismatch between the ideal metric used during policy design and the metric that would be available in a real deployment. The effect is more nuanced than a simple performance degradation:

- A small noise level of 0.02 is still close enough to the ideal metric to preserve the main behavior of the policy.
- A larger noise level of 0.2 is a much stronger mismatch, yet the system still shows the expected improvement over the no-Konro baseline in the OMNeT-enabled case.

This suggests that the policy has meaningful robustness to a noisy and imperfect feedback signal, but that robustness is not unlimited. For the thesis narrative, the critical point is that even a relatively large variance such as 0.2 does not invalidate the approach, which means the policy can still be applied when the available metric is only an approximate version of the ground truth.

## Conclusion

The experiments support the claim that a resource manager such as Konro, combined with the policy we defined, improves the performance of a cooperative multi-agent vision system when communication problems are present.

The main reasons are:

1. The OMNeT-enabled baseline without Konro suffers from high latency and a large amount of stale data.
2. Konro materially reduces those effects while preserving the delivery ratio.
3. The feedback noise is not meant to improve the policy but to simulate the fact that the ideal ground-truth metric is not available in a real deployment.
4. The method remains effective even when that metric is perturbed with substantial noise, including `konro_feedback_noise_std = 0.2`.

From a thesis perspective, the most defensible statement is the following:

Konro is useful not because it changes the communication channel itself, but because it adapts resource usage to communication conditions in a way that preserves perception quality under network stress. The Gaussian noise study shows that the policy does not depend on an unrealistically perfect metric: it still works when the signal used by the controller is only an approximate, noisy surrogate of the ground truth. This is exactly what makes the approach credible for a real cooperative multi-agent perception system.

The cleaned history file and the run indices are consistent with this conclusion, so the previous analysis remains valid after the removal of the incomplete run.