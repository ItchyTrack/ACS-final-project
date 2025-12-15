# Tiered Cuckoo Hash Map for CXL Compressed Memory

A hierarchical cuckoo hash table optimized for Compute Express Link (CXL) hardware-compressed memory systems. This implementation organizes data across multiple tables of exponentially increasing size to exploit compression in sparse regions while maintaining fast access to frequently used entries.

## Overview

This project implements a Tiered Cuckoo Hash Map designed for hardware-compressed memory systems. The structure partitions the hash table into multiple tables of exponentially increasing size. Insertions begin in the smallest table and propagate to larger tables only through cuckoo evictions, concentrating frequently accessed keys in small, cache-resident tables while allowing colder entries to spill into larger, compressed tables.

For detailed analysis and results, see the [full paper](writeup/main.pdf).

## Author

Benjamin Herman  
benherman345@gmail.com
