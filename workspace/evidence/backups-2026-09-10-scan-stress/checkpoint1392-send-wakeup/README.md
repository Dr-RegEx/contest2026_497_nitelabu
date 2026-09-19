# Send buffer notification race repair candidate

TCP/UDP notify retains one semaphore credit when no waiter yet, closing gap after sender releases connection lock before waiting. Configuration identical1387, build/flash/boot/network successful. OriginalUDP1397 running300s40M; firstminute shows samezerotraffic symptom, so this is not established as root-causefix of observedstall. Do not markperformancePASS. PHY MMU deselect1387 remains included.
