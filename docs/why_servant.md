# Why Automatic Spilitting of eBPF Applications??

## Running eBPF in Kernel is Limited!


## Splitting Can Increase performance (or not all tasks should be done in kernel)

*. Results for BMC
*. Results for Katran

### Not all splitting is good

*. Bad results for bad splitting


## Overcoming The eBPF Verifier Limits Is Challenging

* Needs Time
* Needs Expertise
* The Performance is Tied to How You Over Come The Limit

## Payload Limit!

BMC caches values sizes up to 1000 bytes. Caching larger values
can trigger instruction or branching limits. But something strange
that I figured out is that, BMC has a sinister bug that might not
write full responses when preparing the payload response. This
happens if the packet does not have enough space for the response.
The packet header and tail can be adjusted, but the amount of
adjustment is limited and not well documented. I think considering
this fact BMC can not respond with 1000 bytes (or maybe 500 bytes)
responses. At least not reliably. I need to figure this out.
Using Huge pages helps with better adjustment capabilities.
But I am not sure why and how the mechanisms are working.

