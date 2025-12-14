#import "@preview/charged-ieee:0.1.4": ieee

#show: ieee.with(
	title: [CXL Main Memory Compression aware Tiered Cuckoo Hash Map],
	abstract: [
		Curent Cuckoo Hashs do not take advantage of CXL Main Memory Compression. In this paper, I proprose a restructuring the multiple tables in a Cuckoo Hash to take advantage Main Memory Compression. This Tiered Cuckoo Hash Map keeps data densely packed in smaller tables to allow larger compressed tables to take extra values.
	],
	authors: (
		(
		name: "Benjamin	 Herman",
		// department: [],
		// organization: [Typst GmbH],
		location: [Troy, New York],
		email: "benherman345@gmail.com"
		),
	),
	// index-terms: ("Scientific writing", "Typesetting", "Document creation", "Syntax"),
	bibliography: bibliography("refs.bib"),
	figure-supplement: [Fig.],
)

= Introduction



= Design <sec:Design>
The Tiered Cuckoo Hash Map organizes data across a sequence of tables $T_1,...,T_n$, where each table $T_i$ has size $t_s dot t_m^(i-1)$. Insertions begin at $T_1$; displaced entries move to $T_2$ and continue through the tiers. When an entry is displaced from $T_n$, it cycles back to $T_1$, preserving a continuous cuckoo-style eviction chain across all tables. All tables share the same hash function, and the reshuffling of keys is due to the changing table size. When the structure nears its load threshold, it scales by adding a new table $T_(n+1)$ rather than rehashing existing entries.

The design of the Tiered Cuckoo Hash Map prioritizes efficient memory access and cache utilization. Smaller tables at the top of the hierarchy are densely populated, increasing the likelihood that lookups will find their target in the first table checked. By keeping these tables compact, reads and writes can be performed quickly, benefiting from fast cache hits and minimal memory overhead. Larger tables deeper in the hierarchy are much sparser and reside in main memory, where compression is applied automatically by the hardware. These tables are accessed less frequently, so their storage format does not significantly impact performance. All tables use the same hash function, avoiding the need to compute a new hash for each tier. The structure scales by adding new tables rather than resizing and rehashing existing ones. Each new tier is initially sparse and can be efficiently stored in hardware-compressed memory. As tables increase in size across the hierarchy, because I choosed a bucket with the module of the hash and the table size, the existing hash function continues to distribute keys appropriately without modification.

#colbreak()
= Methods <sec:Methods>
To test the effictiveness of this hash map I impelmetned it in C++ @ACSFinalProject. This object was created with parameters and were set before hand and data that I could read. To aid evaluation I benchmarked my results against those of Zhang and I adopt their parameters. @11224718. The complete list of parameters are:
#align(center, table(
	align: left,
	columns: 3,
	inset: (x: 7pt, y: 4pt),
	stroke: (x, y) => if y <= 1 { (top: 0.5pt) },
	fill: (x, y) => if y > 0 and calc.rem(y, 2) == 0  { rgb("#efefef") },
	table.header[Symbol][Definition][Value],
	[$t_s$], [Number of buckets in the table $T_1$], [1024],
	[$t_m$], [Table scale amount per $i$ increment], [4x],
	[$S_(k v)$], [Size of a single KV pair (8B key)], [64B],
	[$n_b$], [Number of KV pairs per bucket], [5 KV],
	[$S_b$], [Total size of a bucket], [264B],
	[$n_(k v)$], [Total number of KV pairs], [],
	[$m_(b c k)$], [Number of buckets], [],
	[$alpha$], [Load factor], [$(n_(k v) dot S_(k v))/(m_(b c k)dot S_b)$],
))
I benchmarked lookup throughput and insert throughput and recored the load factor at this point. I also recored the compression ratio splitting each table into small blocks that each were compressed by LZAV @lzav2025. The sum of the sizes of these compressed blocks divded by the original size of the table gives the compression ratio. The splitting into small blocks is to simulate memory block compression. I used the same two memory block sizes as in the other papar @11224718, 1024B and 4096B.

I made sure that the hash map was running in certain conditions. To inialize the hashmap I insert a minimum of 120000 uniformly random elements to so that there are tables $T_1$ to $T_4$ and insert extra elements to increase the load factor. I olny count statistics that has 4 tables so that there is less noise in the data.

#colbreak()
= Conclusion <sec:Conclusion>

My simulated performance for a Tiered Cuckoo.
#image("imgs/LoadFactor.png")

Zhang's performance for a regular cuckoo map.
#image("imgs/TeresaZhangData.png")
