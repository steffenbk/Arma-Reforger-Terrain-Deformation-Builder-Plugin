# Hole check baseline on v035 voxel ruts (Stage B control run, 2026-10-15)

Run: Workbench log logs_2026-10-15_20-14-47 (20:29:35 - 20:30:49), DISPLACE_RUTS false, Hole check on, standing /
driving in v035 voxel ruts near x 1043-1060, z 2612-2617. DISP_HOLE_TOL was 0.10 m.
76 HOLE lines, 23 distinct spots (table = all of them, last value logged per spot). 0 "hit none", 0 normals < 0.

| x | z | expected | hit | hit - expected | normal y |
|---|---|---|---|---|---|
| 1043.5 | 2612 | 37.794 | 37.691 | -0.104 | 0.955 |
| 1048 | 2612.5 | 37.841 | 37.741 | -0.101 | 0.966 |
| 1050 | 2612 | 37.562 | 37.665 | +0.103 | 0.952 |
| 1050 | 2614 | 37.526 | 37.661 | +0.135 | 0.984 |
| 1050.5 | 2612 | 37.553 | 37.658 | +0.105 | 0.989 |
| 1050.5 | 2613.5 | 37.896 | 37.786 | -0.110 | 0.948 |
| 1051 | 2612.5 | 37.602 | 37.710 | +0.108 | 0.984 |
| 1051.5 | 2614 | 37.896 | 37.775 | -0.121 | 0.939 |
| 1052 | 2612 | 37.896 | 37.770 | -0.126 | 0.931 |
| 1052 | 2612.5 | 37.584 | 37.706 | +0.123 | 0.994 |
| 1052 | 2615.5 | 37.943 | 37.831 | -0.112 | 0.938 |
| 1053 | 2614.5 | 37.927 | 37.820 | -0.107 | 0.954 |
| 1054 | 2613.5 | 37.551 | 37.695 | +0.144 | 0.947 |
| 1054 | 2615 | 37.943 | 37.825 | -0.118 | 0.941 |
| 1054.5 | 2613 | 37.919 | 37.798 | -0.121 | 0.940 |
| 1054.5 | 2616.5 | 37.974 | 37.866 | -0.109 | 0.947 |
| 1055 | 2614.5 | 37.951 | 37.837 | -0.114 | 0.941 |
| 1056.5 | 2614.5 | 37.597 | 37.746 | +0.149 | 0.956 |
| 1057 | 2617 | 37.692 | 37.855 | +0.163 | 0.995 |
| 1058 | 2615 | 37.738 | 37.847 | +0.109 | 0.996 |
| 1058.5 | 2615.5 | 37.692 | 37.792 | +0.101 | 0.965 |
| 1059 | 2615.5 | 37.675 | 37.831 | +0.156 | 0.973 |
| 1059.5 | 2616 | 37.738 | 37.877 | +0.139 | 0.992 |

Checksum: 11 negative + 12 positive rows = 23 spots.

Reading: low expected (rut bottom) -> mesh hits higher (+); high expected (rut edge) -> mesh hits lower (-). The
surface-nets mesh rounds the rut against the bilinear column-surface estimate. Not holes. Worst |diff| 0.163 m.
-> DISP_HOLE_TOL 0.25. A real hole shows as "hit none", a normal y < 0, or |diff| past 0.25.
Not measured: the total check count (Summary log was off, so the hole rate per check is unknown).
