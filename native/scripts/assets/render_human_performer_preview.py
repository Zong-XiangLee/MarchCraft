#!/usr/bin/env python3
"""Render a dependency-free SVG QA contact sheet from the conditioned skin."""

from __future__ import annotations

import argparse
import html
import math
import pathlib
import struct

from build_human_performer import JOINTS, read_accessor, read_glb

SOLE_TABLES = (
    (-0.00070, 0.00006, 0.00160, 0.00109, -0.00008, -0.00002, -0.00002, -0.00001, -0.00000, -0.00024, -0.00047, -0.00066, -0.00076, -0.00059, -0.00057, -0.00067, -0.00070, 0.00006, 0.00160, 0.00109, -0.00008, -0.00002, -0.00002, -0.00001, -0.00000, -0.00024, -0.00047, -0.00066, -0.00076, -0.00059, -0.00057, -0.00067),
    (-0.00256, -0.00126, -0.00270, -0.00374, -0.00145, -0.00070, -0.00041, -0.00076, -0.00199, -0.00321, -0.00443, -0.00570, -0.00678, -0.00637, -0.00484, -0.00352, -0.00945, -0.00342, -0.00306, -0.00470, -0.00360, -0.00466, -0.00360, -0.00261, -0.00170, -0.00086, -0.00033, -0.00055, -0.00070, -0.00124, -0.00161, -0.00196),
    (-0.02222, -0.00881, 0.00083, 0.00025, 0.00004, -0.00162, -0.00458, -0.00712, -0.00719, -0.00460, -0.00189, 0.00074, 0.00093, 0.00120, 0.00172, 0.00241, 0.00239, 0.00339, 0.00343, 0.00194, 0.00168, 0.00110, -0.00152, -0.00400, -0.00664, -0.00945, -0.00397, -0.00455, -0.01469, -0.01606, -0.01172, -0.01483),
    (0.01142, 0.01086, 0.00738, 0.00370, 0.00252, 0.00414, 0.00620, 0.00831, 0.01050, 0.01265, 0.01280, 0.01295, 0.01308, 0.01322, 0.01340, 0.01361, 0.01307, 0.01279, 0.01282, 0.01288, 0.01291, 0.01286, 0.01277, 0.01264, 0.01079, 0.00844, 0.00599, 0.00344, 0.00122, 0.00197, 0.00595, 0.01017),
    (0.01249, 0.01248, 0.01247, 0.01247, 0.01246, 0.01247, 0.01247, 0.01248, 0.01248, 0.01247, 0.01244, 0.01239, 0.01237, 0.01240, 0.01213, 0.01184, 0.01249, 0.01248, 0.01247, 0.01247, 0.01246, 0.01247, 0.01247, 0.01248, 0.01248, 0.01247, 0.01244, 0.01239, 0.01237, 0.01240, 0.01213, 0.01184),
    (0.01307, 0.01280, 0.01284, 0.01290, 0.01292, 0.01288, 0.01278, 0.01264, 0.01067, 0.00824, 0.00573, 0.00313, 0.00090, 0.00174, 0.00585, 0.01015, 0.01142, 0.01086, 0.00746, 0.00392, 0.00284, 0.00445, 0.00647, 0.00852, 0.01062, 0.01264, 0.01280, 0.01294, 0.01307, 0.01321, 0.01339, 0.01361),
    (0.00239, 0.00339, 0.00344, 0.00198, 0.00171, 0.00103, -0.00163, -0.00426, -0.00706, -0.01000, -0.00409, -0.00505, -0.01539, -0.01657, -0.01232, -0.01517, -0.02222, -0.00846, 0.00089, 0.00028, 0.00006, -0.00160, -0.00457, -0.00740, -0.00676, -0.00433, -0.00177, 0.00073, 0.00091, 0.00119, 0.00171, 0.00241),
    (-0.00945, -0.00341, -0.00304, -0.00466, -0.00353, -0.00480, -0.00374, -0.00274, -0.00181, -0.00096, -0.00034, -0.00055, -0.00073, -0.00123, -0.00160, -0.00195, -0.00256, -0.00128, -0.00275, -0.00378, -0.00144, -0.00068, -0.00040, -0.00065, -0.00187, -0.00307, -0.00428, -0.00555, -0.00665, -0.00628, -0.00482, -0.00353),
)
ZERO_RESIDUAL = (0.0,) * 16
HALF_STRIDE_CORRECTIONS = (
    (-0.00000, 0.00062, -0.00000, 0.00001, 0.00015, -0.00000, -0.00000, -0.00000, -0.00000, 0.00006, 0.00011, 0.00014, 0.00013, -0.00004, -0.00007, -0.00000, -0.00000, 0.00062, -0.00000, 0.00001, 0.00015, -0.00000, -0.00000, -0.00000, -0.00000, 0.00006, 0.00011, 0.00014, 0.00013, -0.00004, -0.00007, -0.00000),
    (0.00101, 0.00100, 0.00177, 0.00267, 0.00181, 0.00017, -0.00034, -0.00063, 0.00000, 0.00064, 0.00131, 0.00203, 0.00274, 0.00295, 0.00287, 0.00277, 0.00894, 0.00361, 0.00220, 0.00265, 0.00177, 0.00187, 0.00123, 0.00061, 0.00000, -0.00057, -0.00087, -0.00043, 0.00009, 0.00050, 0.00064, 0.00056),
    (0.02041, 0.00723, -0.00051, -0.00010, -0.00008, -0.00036, -0.00041, -0.00139, 0.00000, -0.00122, -0.00250, -0.00367, -0.00229, -0.00086, -0.00047, -0.00118, -0.00124, -0.00127, -0.00221, -0.00187, -0.00111, -0.00188, -0.00212, -0.00110, 0.00000, 0.00119, -0.00581, -0.00059, 0.00843, 0.00469, 0.00480, 0.01197),
    (0.00019, 0.00078, 0.00224, 0.00370, 0.00405, 0.00318, 0.00214, 0.00108, -0.00000, -0.00101, 0.00001, 0.00001, 0.00000, -0.00003, -0.00013, -0.00048, 0.00017, 0.00042, 0.00035, 0.00023, 0.00012, 0.00006, 0.00002, -0.00058, 0.00000, 0.00103, 0.00211, 0.00326, 0.00429, 0.00400, 0.00225, 0.00030),
    (0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, -0.00000, -0.00006, -0.00011, -0.00014, -0.00019, -0.00028, -0.00017, -0.00002, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, -0.00000, -0.00006, -0.00011, -0.00014, -0.00019, -0.00028, -0.00017, -0.00002),
    (0.00017, 0.00042, 0.00034, 0.00022, 0.00012, 0.00006, 0.00002, -0.00065, -0.00000, 0.00107, 0.00219, 0.00337, 0.00441, 0.00410, 0.00229, 0.00031, 0.00019, 0.00078, 0.00222, 0.00362, 0.00395, 0.00308, 0.00206, 0.00104, 0.00000, -0.00093, 0.00001, 0.00001, -0.00000, -0.00003, -0.00013, -0.00048),
    (-0.00124, -0.00126, -0.00221, -0.00187, -0.00109, -0.00177, -0.00214, -0.00117, 0.00000, 0.00126, -0.00554, -0.00025, 0.00876, 0.00488, 0.00527, 0.01230, 0.02041, 0.00690, -0.00056, -0.00011, -0.00007, -0.00036, -0.00042, -0.00061, -0.00000, -0.00115, -0.00237, -0.00350, -0.00219, -0.00083, -0.00048, -0.00119),
    (0.00894, 0.00361, 0.00218, 0.00262, 0.00172, 0.00196, 0.00125, 0.00062, -0.00000, -0.00058, -0.00096, -0.00051, 0.00010, 0.00048, 0.00061, 0.00054, 0.00101, 0.00102, 0.00178, 0.00268, 0.00181, 0.00016, -0.00025, -0.00062, -0.00000, 0.00063, 0.00129, 0.00200, 0.00271, 0.00292, 0.00286, 0.00277),
)
EXTENDED_STRIDE_CORRECTIONS = (
    (-0.02118, -0.00459, 0.00000, -0.00000, -0.00004, -0.00000, -0.00000, 0.00000, -0.00000, -0.00003, -0.00005, -0.00007, -0.00005, 0.00007, 0.00007, -0.01028, -0.02118, -0.00459, 0.00000, -0.00000, -0.00004, -0.00000, -0.00000, 0.00000, -0.00000, -0.00003, -0.00005, -0.00007, -0.00005, 0.00007, 0.00007, -0.01028),
    (-0.01123, -0.00129, -0.00122, -0.00018, -0.00162, -0.00013, -0.00005, 0.00028, 0.00000, -0.00029, -0.00062, -0.00101, -0.00144, -0.00169, -0.01014, -0.02575, -0.03297, -0.02044, -0.00178, 0.00028, -0.00730, -0.00082, -0.00055, -0.00027, 0.00000, 0.00026, -0.00002, -0.00009, 0.00002, -0.00054, -0.00084, -0.00145),
    (-0.03235, -0.02731, -0.00476, 0.00030, 0.00002, 0.00005, 0.00013, 0.00160, 0.00000, 0.00055, 0.00114, -0.00004, -0.00000, 0.00012, 0.00047, 0.00101, -0.00227, 0.00120, 0.00117, 0.00124, 0.00138, -0.00058, 0.00094, 0.00049, 0.00000, -0.00054, 0.00140, -0.00201, -0.00562, -0.00549, -0.01687, -0.02684),
    (-0.02136, -0.01026, -0.00078, -0.00142, -0.00162, -0.00126, -0.00083, -0.00042, -0.00000, -0.00001, -0.00001, -0.00003, -0.00004, -0.00008, -0.00006, -0.00003, -0.00006, -0.00049, -0.00040, -0.00026, -0.00014, -0.00007, -0.00003, -0.00001, 0.00000, -0.00041, -0.00086, -0.00130, -0.00170, -0.00139, -0.00040, -0.00907),
    (-0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00001, -0.00000, 0.00003, 0.00005, 0.00007, 0.00010, 0.00010, 0.00012, 0.00001, -0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00000, -0.00001, -0.00000, 0.00003, 0.00005, 0.00007, 0.00010, 0.00010, 0.00012, 0.00001),
    (-0.00016, -0.00056, -0.00041, -0.00026, -0.00014, -0.00006, -0.00002, -0.00000, -0.00000, -0.00042, -0.00086, -0.00130, -0.00168, -0.00138, -0.00037, -0.00923, -0.02136, -0.01009, -0.00077, -0.00147, -0.00173, -0.00131, -0.00084, -0.00042, 0.00000, -0.00000, -0.00000, -0.00002, -0.00004, -0.00008, -0.00006, -0.00008),
    (-0.00227, 0.00119, 0.00115, 0.00121, 0.00132, -0.00049, 0.00100, 0.00052, 0.00000, 0.00021, 0.00137, -0.00216, -0.00576, -0.00584, -0.01701, -0.02693, -0.03235, -0.02721, -0.00400, 0.00033, 0.00002, 0.00004, 0.00013, 0.00190, -0.00000, 0.00052, 0.00108, -0.00004, -0.00000, 0.00013, 0.00048, 0.00102),
    (-0.03297, -0.02049, -0.00181, 0.00033, -0.00738, -0.00083, -0.00056, -0.00028, -0.00000, 0.00026, -0.00002, -0.00009, 0.00011, -0.00053, -0.00083, -0.00142, -0.01123, -0.00130, -0.00124, -0.00013, -0.00163, -0.00013, -0.00005, 0.00027, -0.00000, -0.00029, -0.00061, -0.00100, -0.00142, -0.00168, -0.01006, -0.02569),
)
HALF_BACK_DIAGONAL_CORRECTIONS = (
    (0.01282, 0.01178, 0.00557, 0.00081, -0.00107, 0.00202, 0.00672, 0.01200, 0.01249, 0.01298, 0.01347, 0.01395, 0.01437, 0.01327, 0.01253, 0.01268, 0.01261, 0.01161, 0.01244, 0.01351, 0.01284, 0.01241, 0.01342, 0.01295, 0.01248, 0.01202, 0.01140, 0.00456, 0.00398, 0.01082, 0.01156, 0.01234),
    (0.01177, 0.01066, 0.00436, -0.00054, -0.00231, 0.00089, 0.00562, 0.01088, 0.01169, 0.01249, 0.01327, 0.01412, 0.01379, 0.01237, 0.01167, 0.01181, 0.01182, 0.01088, 0.01163, 0.01259, 0.01199, 0.01273, 0.01324, 0.01244, 0.01168, 0.01093, 0.01003, 0.00336, 0.00255, 0.00897, 0.01014, 0.01138),
    (0.01182, 0.01088, 0.01163, 0.01257, 0.01196, 0.01257, 0.01316, 0.01245, 0.01168, 0.01092, 0.00938, 0.00299, 0.00241, 0.00896, 0.01015, 0.01139, 0.01177, 0.01068, 0.00449, -0.00020, -0.00182, 0.00142, 0.00614, 0.01089, 0.01169, 0.01247, 0.01325, 0.01409, 0.01370, 0.01235, 0.01168, 0.01182),
    (0.01261, 0.01161, 0.01244, 0.01349, 0.01274, 0.01224, 0.01332, 0.01295, 0.01248, 0.01202, 0.01071, 0.00416, 0.00383, 0.01081, 0.01156, 0.01234, 0.01282, 0.01179, 0.00570, 0.00116, -0.00055, 0.00258, 0.00728, 0.01200, 0.01249, 0.01297, 0.01345, 0.01393, 0.01435, 0.01325, 0.01254, 0.01268),
)
EXTENDED_BACK_DIAGONAL_CORRECTIONS = (
    (0.02165, 0.01619, -0.00580, -0.00869, -0.01078, -0.00571, 0.00148, 0.00665, 0.01249, 0.01360, 0.01349, 0.01242, 0.01259, 0.01258, 0.01230, 0.01005, 0.01168, 0.00708, 0.00620, 0.00850, 0.00947, 0.01226, 0.01349, 0.01362, 0.01248, 0.01153, -0.00039, -0.00102, 0.00882, 0.01268, 0.02005, 0.02308),
    (0.01367, 0.00925, -0.01025, -0.01126, -0.01256, -0.00711, 0.00040, 0.00594, 0.01208, 0.01393, 0.01285, 0.01182, 0.01190, 0.01167, 0.01099, 0.00815, 0.00905, 0.00577, 0.00666, 0.00852, 0.00918, 0.01169, 0.01282, 0.01391, 0.01204, 0.01054, -0.00116, -0.00249, 0.00600, 0.00915, 0.01383, 0.01558),
    (0.00901, 0.00579, 0.00671, 0.00857, 0.00925, 0.01170, 0.01288, 0.01395, 0.01206, 0.00996, -0.00143, -0.00230, 0.00643, 0.00955, 0.01421, 0.01580, 0.01369, 0.00905, -0.01073, -0.01095, -0.01228, -0.00674, 0.00074, 0.00630, 0.01205, 0.01390, 0.01282, 0.01183, 0.01190, 0.01167, 0.01100, 0.00813),
    (0.01163, 0.00704, 0.00624, 0.00854, 0.00953, 0.01226, 0.01355, 0.01364, 0.01248, 0.01082, -0.00084, -0.00102, 0.00905, 0.01316, 0.02025, 0.02319, 0.02166, 0.01614, -0.00594, -0.00817, -0.00994, -0.00494, 0.00206, 0.00716, 0.01249, 0.01359, 0.01345, 0.01244, 0.01259, 0.01260, 0.01233, 0.01004),
)
EXTENDED_RESIDUALS = (
    (0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000),
    (0.00166, 0.00115, 0.00064, 0.00041, 0.00017, -0.00000, -0.00017, -0.00008, 0.00000, 0.00011, 0.00022, 0.00006, -0.00010, -0.00020, -0.00030, 0.00132, 0.00294, 0.00231, 0.00168, 0.00106, 0.00044, 0.00024, 0.00005, 0.00002, -0.00000, -0.00020, -0.00041, -0.00077, -0.00112, -0.00155, -0.00197, -0.00016),
    (0.00037, -0.00010, -0.00058, -0.00068, -0.00078, -0.00082, -0.00087, -0.00063, -0.00039, -0.00024, -0.00008, -0.00015, -0.00023, -0.00016, -0.00009, 0.00177, 0.00363, 0.00263, 0.00163, 0.00098, 0.00034, 0.00006, -0.00022, -0.00029, -0.00036, -0.00072, -0.00108, -0.00148, -0.00187, -0.00227, -0.00266, -0.00115),
    (-0.00094, -0.00067, -0.00040, -0.00034, -0.00029, -0.00022, -0.00014, -0.00007, 0.00000, 0.00000, 0.00001, 0.00003, 0.00005, 0.00012, 0.00020, 0.00033, 0.00046, 0.00026, 0.00006, 0.00005, 0.00003, 0.00002, 0.00001, 0.00001, -0.00000, -0.00005, -0.00010, -0.00019, -0.00028, -0.00041, -0.00054, -0.00074),
    (0.00312, -0.00044, -0.00121, -0.00008, 0.00041, 0.00029, 0.00021, 0.00015, 0.00012, 0.00000, 0.00002, 0.00009, 0.00019, 0.00041, 0.00123, 0.00175, 0.00147, -0.00087, -0.00162, -0.00087, -0.00039, -0.00017, -0.00005, -0.00000, 0.00012, 0.00007, -0.00002, -0.00012, -0.00015, 0.00016, 0.00082, 0.00234),
    (0.00010, -0.00081, -0.00050, -0.00023, -0.00008, -0.00003, -0.00001, 0.00000, 0.00000, 0.00000, 0.00002, 0.00006, 0.00009, 0.00014, 0.00053, 0.00061, 0.00007, -0.00085, -0.00060, -0.00033, -0.00023, -0.00018, -0.00008, -0.00002, 0.00000, -0.00002, -0.00007, -0.00013, -0.00017, -0.00001, 0.00023, 0.00060),
    (0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000),
    (0.00007, -0.00085, -0.00060, -0.00033, -0.00023, -0.00018, -0.00008, -0.00002, 0.00000, -0.00002, -0.00007, -0.00013, -0.00017, -0.00001, 0.00023, 0.00060, 0.00010, -0.00081, -0.00050, -0.00023, -0.00008, -0.00003, -0.00001, 0.00000, 0.00000, 0.00000, 0.00002, 0.00006, 0.00009, 0.00014, 0.00053, 0.00061),
    (0.00148, -0.00087, -0.00162, -0.00087, -0.00039, -0.00017, -0.00005, -0.00000, 0.00013, 0.00008, -0.00001, -0.00011, -0.00015, 0.00016, 0.00081, 0.00235, 0.00312, -0.00045, -0.00122, -0.00009, 0.00042, 0.00029, 0.00020, 0.00014, 0.00011, 0.00000, 0.00002, 0.00009, 0.00019, 0.00041, 0.00123, 0.00175),
    (0.00056, 0.00032, 0.00008, 0.00006, 0.00004, 0.00002, -0.00000, -0.00000, -0.00000, -0.00006, -0.00013, -0.00024, -0.00035, -0.00047, -0.00060, -0.00077, -0.00094, -0.00068, -0.00041, -0.00027, -0.00013, -0.00011, -0.00010, -0.00005, 0.00000, 0.00000, -0.00000, 0.00002, 0.00005, 0.00013, 0.00021, 0.00038),
    (0.00368, 0.00265, 0.00162, 0.00098, 0.00033, 0.00005, -0.00022, -0.00030, -0.00038, -0.00088, -0.00139, -0.00186, -0.00233, -0.00268, -0.00303, -0.00134, 0.00035, 0.00014, -0.00008, -0.00014, -0.00019, -0.00033, -0.00047, -0.00041, -0.00036, -0.00023, -0.00011, -0.00017, -0.00023, -0.00017, -0.00010, 0.00179),
    (0.00299, 0.00234, 0.00170, 0.00107, 0.00044, 0.00024, 0.00005, 0.00003, 0.00000, -0.00029, -0.00057, -0.00097, -0.00138, -0.00179, -0.00219, -0.00027, 0.00165, 0.00116, 0.00067, 0.00043, 0.00019, 0.00011, 0.00002, 0.00001, -0.00000, 0.00010, 0.00020, 0.00005, -0.00010, -0.00021, -0.00032, 0.00134),
    (0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000, 0.00000),
)


def identity():
    return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def multiply(a, b):
    return [[sum(a[row][k] * b[k][column] for k in range(4)) for column in range(4)] for row in range(4)]


def translation(x, y, z):
    matrix = identity()
    matrix[0][3], matrix[1][3], matrix[2][3] = x, y, z
    return matrix


def rotation(x_degrees=0.0, y_degrees=0.0, z_degrees=0.0):
    x, y, z = (math.radians(value) for value in (x_degrees, y_degrees, z_degrees))
    rx = [[1,0,0,0], [0,math.cos(x),-math.sin(x),0], [0,math.sin(x),math.cos(x),0], [0,0,0,1]]
    ry = [[math.cos(y),0,math.sin(y),0], [0,1,0,0], [-math.sin(y),0,math.cos(y),0], [0,0,0,1]]
    rz = [[math.cos(z),-math.sin(z),0,0], [math.sin(z),math.cos(z),0,0], [0,0,1,0], [0,0,0,1]]
    return multiply(multiply(rz, ry), rx)


def transform(matrix, point):
    x, y, z = point
    return tuple(sum(matrix[row][k] * (x, y, z, 1.0)[k] for k in range(4)) for row in range(3))


def clamp(value, low, high):
    return max(low, min(high, value))


def mix(a, b, amount):
    return a + (b - a) * amount


def smoother_step(value):
    value = clamp(value, 0.0, 1.0)
    return value ** 3 * (value * (value * 6.0 - 15.0) + 10.0)


def leg_cycle(phase, left):
    return (phase + (0.5 if left else 0.0)) % 1.0


def foot_pitch(mode, cycle):
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    if mode == "march.forward":
        if stance:
            if t < 0.30:
                return mix(32.0, 0.0, smoother_step(t / 0.30))
            return 0.0
        if t < 0.20:
            return mix(0.0, -16.0, smoother_step(t / 0.20))
        if t < 0.50:
            return mix(-16.0, 0.0, smoother_step((t - 0.20) / 0.30))
        if t < 0.76:
            return 0.0
        return mix(0.0, 32.0, smoother_step((t - 0.76) / 0.24))
    if mode == "march.backward":
        return -8.0 if stance else -8.0 + 2.0 * math.sin(math.pi * t)
    return 0.0


def toe_pitch(mode, cycle):
    return 0.0


def sole_lift(pitch, toe_extent):
    radians = math.radians(pitch)
    return max(0.0, -math.sin(radians) * 0.17, math.sin(radians) * toe_extent)


def leg_target(mode, phase, left, stride):
    cycle = leg_cycle(phase, left)
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    travel = t if stance else smoother_step(t)
    direction = 1.0 if mode in ("march.backward", "slide.right") else -1.0
    along = direction * stride * ((0.5 - travel) if stance else (travel - 0.5))
    lift = 0.0
    if not stance:
        if mode == "march.forward":
            if t < 0.20:
                lift = 0.022 * smoother_step(t / 0.20)
            elif t < 0.55:
                lift = mix(0.022, 0.003, smoother_step((t - 0.20) / 0.35))
            elif t < 0.72:
                lift = mix(0.003, 0.0, smoother_step((t - 0.55) / 0.17))
        else:
            arc = math.sin(math.pi * t) ** 1.35
            lift = arc * (0.003 if mode == "march.backward" else 0.018)
    pitch = foot_pitch(mode, cycle)
    toe_extent = 0.08 if mode == "march.backward" else 0.0
    lift += sole_lift(pitch, toe_extent)
    sagittal = mode in ("march.forward", "march.backward")
    return {"stance": stance, "t": t, "x": 0.0 if sagittal else along,
            "z": along if sagittal else 0.0, "lift": lift,
            "foot": pitch, "toe": toe_pitch(mode, cycle)}


def pelvis_transfer_pose(targets, pelvis_yaw=0.0, spatial=False):
    reach = 0.39 + 0.415 - 0.001

    def available(target):
        horizontal = math.hypot(target["x"], target["z"])
        return math.sqrt(max(0.0, reach * reach - horizontal * horizontal)) + target["lift"]

    support = next(target for target in targets if target["stance"])
    moving = next(target for target in targets if not target["stance"])

    def effective_point(target, left):
        if not spatial:
            return target["x"], target["z"]
        yaw = math.radians(pelvis_yaw)
        hip_bind = -0.105 if left else 0.105
        return (target["x"] + hip_bind - math.cos(yaw) * hip_bind,
                target["z"] + math.sin(yaw) * hip_bind)

    support_point = effective_point(support, support is targets[0])
    moving_point = effective_point(moving, moving is targets[0])
    support_adjustment = clamp(available(support) - 0.805, -0.06, 0.05)
    if 0.30 <= moving["t"] <= 0.70:
        return {"x": 0.0, "y": support_adjustment, "z": 0.0, "weight": 0.0}
    delta_x = support_point[0] - moving_point[0]
    delta_z = support_point[1] - moving_point[1]
    horizontal_distance = math.hypot(delta_x, delta_z)
    ankle_height_delta = support["lift"] - moving["lift"]
    ankle_distance = math.hypot(horizontal_distance, ankle_height_delta)
    if horizontal_distance < 0.0001 or ankle_distance >= reach * 2.0:
        return {"x": 0.0, "y": support_adjustment, "z": 0.0, "weight": 0.0}
    sphere_height = math.sqrt(max(
        0.0, reach * reach - ankle_distance * ankle_distance * 0.25))
    distance_along_ground = (horizontal_distance * 0.5
                             - ankle_height_delta / ankle_distance * sphere_height)
    hip_height = ((moving["lift"] + support["lift"]) * 0.5
                  + horizontal_distance / ankle_distance * sphere_height)
    transfer_weight = (1.0 - smoother_step(moving["t"] / 0.30)
                       if moving["t"] < 0.30
                       else smoother_step((moving["t"] - 0.70) / 0.30))
    return {
        "x": (moving_point[0] + delta_x / horizontal_distance * distance_along_ground)
             * transfer_weight,
        "y": mix(support_adjustment, clamp(hip_height - 0.805, -0.06, 0.05),
                 transfer_weight),
        "z": (moving_point[1] + delta_z / horizontal_distance * distance_along_ground)
             * transfer_weight,
        "weight": transfer_weight,
    }


def body_pose(mode, phase, stride):
    if mode == "idle":
        return {"pelvis_x": 0.0, "pelvis_y": -0.00191, "pelvis_z": 0.0,
                "ik_pelvis_y": -0.00191,
                "pelvis_yaw": 0.0,
                "pelvis_roll": 0.0, "spine_yaw": 0.0, "spine_roll": 0.0,
                "spine_pitch": 1.6, "spine_lift": 0.0065645,
                "spine_x": 0.0, "spine_z": 0.0,
                "head_pitch": 10.0}
    targets = [leg_target(mode, phase, left, stride) for left in (True, False)]
    support = next(target for target in targets if target["stance"])
    transfer = pelvis_transfer_pose(targets)
    drop = transfer["y"]
    rhythm = phase * math.pi * 2.0
    slide = 1.0 if mode == "slide.right" else -1.0 if mode == "slide.left" else 0.0
    if slide:
        support_is_left = targets[0]["stance"]
        normalized_reach = support["x"] / (stride * 0.5) if stride > 0.0001 else 0.0
        if slide > 0:
            sole_factor = ((0.012 + 0.010 * normalized_reach) if support_is_left
                           else (0.026 + 0.009 * normalized_reach))
        else:
            sole_factor = ((0.025 - 0.010 * normalized_reach) if support_is_left
                           else (0.010 - 0.010 * normalized_reach))
        drop += abs(support["x"]) * max(0.0, sole_factor)
        if slide < 0.0:
            drop += 0.00002 * clamp(stride / 0.70, 0.0, 1.0)
    yaw = slide * (60.0 + math.sin(rhythm)) if slide else math.sin(rhythm) * 1.4
    roll = -math.cos(rhythm) * 0.38
    yaw_radians = math.radians(yaw)
    return {"pelvis_x": transfer["x"]
            + math.cos(rhythm) * 0.0065 * (1.0 - transfer["weight"]),
            "pelvis_y": drop, "pelvis_z": transfer["z"],
            "ik_pelvis_y": drop,
            "pelvis_yaw": yaw, "pelvis_roll": roll,
            "spine_yaw": -yaw * (1.0 if slide else 0.78), "spine_roll": -roll * 0.92,
            "spine_pitch": math.sin(rhythm + 0.18) * 0.22, "spine_lift": -drop * 0.95,
            "spine_x": (-math.cos(yaw_radians) * transfer["x"]
                        + math.sin(yaw_radians) * transfer["z"]),
            "spine_z": (-math.sin(yaw_radians) * transfer["x"]
                        - math.cos(yaw_radians) * transfer["z"]),
            "head_pitch": 10.0 - math.sin(rhythm + 0.18) * 0.12}


def directional_weights(angle_degrees):
    radians = math.radians(((angle_degrees + 180.0) % 360.0) - 180.0)
    longitudinal, lateral = abs(math.cos(radians)), abs(math.sin(radians))
    quadrant_degrees = math.degrees(math.atan2(lateral, longitudinal))
    lateral_weight = smoother_step((quadrant_degrees - 22.5) / 45.0)
    longitudinal_weight = 1.0 - lateral_weight
    return [longitudinal_weight if math.cos(radians) >= 0.0 else 0.0,
            longitudinal_weight if math.cos(radians) < 0.0 else 0.0,
            lateral_weight if math.sin(radians) < 0.0 else 0.0,
            lateral_weight if math.sin(radians) >= 0.0 else 0.0]


def backward_technique_weight(angle_degrees):
    rear_angle = abs(((angle_degrees + 180.0) % 360.0) - 180.0)
    return smoother_step((rear_angle - 90.0) / 15.0)


SLIDE_CONTACT_90 = [0.000603,-0.0006319,-0.0011648,-0.0007321,0.0006029,0.00367,0.0028282,0.0026481,0.0005955,0.0005864,0.0005411,0.0005321,0.000597,0.0006308,0.0006761,0.0006796,0.0006036,0.0007851,0.0008462,0.0007816,0.0005974,0.0006392,0.0006176,0.0005837,0.0006006,0.0006329,0.0008569,0.0013463,0.0006012,-0.0013342,-0.0006974,-0.0000544,0.000595,0.000586,0.0005838,0.0005885,0.0006002,0.000589,0.000585,0.0005883,0.0005987,0.0006365,0.0006815,0.0007337,0.0006002,0.0005969,0.0005953,0.0005951,0.0005975,0.0005846,0.000579,0.0005837,0.000602,0.0005741,0.0005642,0.0005724,0.0005972,0.0005926,0.000596,0.0006011,0.0006005,0.0007325,0.0005712,0.0005424,0.0006015,0.0003191,0.0002284,0.0004652,0.0006009,0.0008673,0.0009981,0.0009104,0.0005979,0.0005215,0.0004275,0.0004349,0.0005987,0.0005792,0.0005998,0.0006136,0.0005975,0.000623,0.0006146,0.0005901,0.0005969,0.0003888,0.000471,0.0005428,0.0006042,0.00062,0.0006254,0.0006201,0.0006043,0.000618,0.0006212,0.0006141,0.0005967,0.0006118,0.000617,0.0006125,0.0005985,-0.0008329,0.0000446,0.0005521,0.0005956,0.0015994,0.0019699,0.0016484,0.0006026,0.0012229,0.0012246,0.0009044,0.0005977,-0.0015882,-0.0017899,-0.0007731,0.0006007,0.0008646,0.0013868,0.0015296,0.0005999,0.0011529,0.0013223,0.0011269,0.0006022,0.0008675,0.0009118,0.0008014]
SLIDE_CONTACT_270 = [0.0006015,0.0003201,0.0002313,0.0004719,0.0006005,0.0008662,0.0009981,0.000913,0.0006046,0.0005277,0.0004326,0.0004374,0.0005968,0.000581,0.0006041,0.0006184,0.0005995,0.0006401,0.0006399,0.0006131,0.0005969,0.000461,0.0005203,0.0005679,0.0006038,0.0006206,0.0006261,0.0006205,0.0006039,0.0006192,0.0006242,0.0006192,0.0006045,0.0006155,0.0006177,0.0006115,0.0005973,-0.0002004,0.0004687,0.0007651,0.0005967,0.0015924,0.0019583,0.0016379,0.0006012,0.0011959,0.0011859,0.0008752,0.0006037,-0.001617,-0.0018256,-0.0007968,0.000598,0.0009587,0.0015792,0.001468,0.0006041,0.0011438,0.0013081,0.001115,0.0005993,0.0008589,0.0009023,0.0007948,0.000603,-0.0006279,-0.0011598,-0.0007287,0.0006025,0.003823,0.0027649,0.0027213,0.0005983,0.0005905,0.0005439,0.0005326,0.0005958,0.0006245,0.0006666,0.0006695,0.0005954,0.0007783,0.0008419,0.0007801,0.0005982,0.0006433,0.0006234,0.0005894,0.0006049,0.0007055,0.0009989,0.0015623,0.0006,-0.0006938,-0.0002686,0.000161,0.0005958,0.0005885,0.0005871,0.0005922,0.0006041,0.0005906,0.0005846,0.0005865,0.0005966,0.0006301,0.0006723,0.0007234,0.0005962,0.0005953,0.0005958,0.0005977,0.0006023,0.0005867,0.0005785,0.000581,0.0005975,0.0005707,0.0005623,0.0005725,0.0005994,0.0005946,0.0005976,0.0006019,0.0006001,0.0007261,0.0005684,0.0005413]


def directional_sole_offset(angle_degrees, phase, stride):
    angle = angle_degrees % 360.0
    quadrant = int(math.floor(angle / 90.0)) % 4
    longitudinal = SOLE_TABLES[0] if quadrant in (0, 3) else SOLE_TABLES[4]
    side = SOLE_TABLES[2] if quadrant < 2 else SOLE_TABLES[6]
    diagonal = SOLE_TABLES[(quadrant * 2 + 1) % 8]

    def sampled(table):
        position = (phase % 1.0) * len(table)
        index = int(math.floor(position)) % len(table)
        return mix(table[index], table[(index + 1) % len(table)], position - math.floor(position))

    weights = directional_weights(angle_degrees)
    lateral_weight = weights[2] + weights[3]
    base = (mix(sampled(longitudinal), sampled(diagonal), lateral_weight * 2.0)
            if lateral_weight <= 0.5
            else mix(sampled(diagonal), sampled(side), (lateral_weight - 0.5) * 2.0))
    def correction_from(tables):
        longitudinal_correction = tables[0] if quadrant in (0, 3) else tables[4]
        side_correction = tables[2] if quadrant < 2 else tables[6]
        diagonal_correction = tables[(quadrant * 2 + 1) % 8]
        return (mix(sampled(longitudinal_correction), sampled(diagonal_correction), lateral_weight * 2.0)
                if lateral_weight <= 0.5
                else mix(sampled(diagonal_correction), sampled(side_correction),
                         (lateral_weight - 0.5) * 2.0))
    def back_diagonal_correction(tables):
        zero = (0.0,) * 8
        if 90.0 <= angle <= 135.0:
            if angle <= 105.0:
                first, second, amount = zero, tables[0], (angle - 90.0) / 15.0
            elif angle <= 120.0:
                first, second, amount = tables[0], tables[1], (angle - 105.0) / 15.0
            else:
                first, second, amount = tables[1], zero, (angle - 120.0) / 15.0
        elif 225.0 <= angle <= 270.0:
            if angle <= 240.0:
                first, second, amount = zero, tables[2], (angle - 225.0) / 15.0
            elif angle <= 255.0:
                first, second, amount = tables[2], tables[3], (angle - 240.0) / 15.0
            else:
                first, second, amount = tables[3], zero, (angle - 255.0) / 15.0
        else:
            return 0.0
        return mix(sampled(first), sampled(second), amount)
    if stride < 0.5715:
        base += (correction_from(HALF_STRIDE_CORRECTIONS)
                 + back_diagonal_correction(HALF_BACK_DIAGONAL_CORRECTIONS)) * clamp((0.5715 - stride) / 0.28575, 0.0, 1.0)
    elif stride > 0.5715:
        base += (correction_from(EXTENDED_STRIDE_CORRECTIONS)
                 + back_diagonal_correction(EXTENDED_BACK_DIAGONAL_CORRECTIONS)) * clamp((stride - 0.5715) / 0.1285, 0.0, 1.0)
    # Match the runtime's contact margin between calibration samples.
    base -= 0.0006 * lateral_weight
    contact_weight = max(0.0, lateral_weight * 2 - 1) * (1 - clamp(abs(stride - 0.5715) / 0.1285, 0.0, 1.0))
    base += sampled(SLIDE_CONTACT_90 if quadrant < 2 else SLIDE_CONTACT_270) * contact_weight
    extended_weight = clamp((stride - 0.5715) / 0.1285, 0.0, 1.0)
    if extended_weight <= 0.0 or angle < 90.0 or angle > 270.0:
        return base
    residual_position = (angle - 90.0) / 15.0
    residual_index = min(11, int(math.floor(residual_position)))
    residual = mix(sampled(EXTENDED_RESIDUALS[residual_index]),
                   sampled(EXTENDED_RESIDUALS[residual_index + 1]),
                   residual_position - residual_index)
    return base + residual * extended_weight


def directional_target(angle_degrees, phase, left, stride):
    cycle = leg_cycle(phase, left)
    stance = cycle < 0.5
    t = cycle * 2.0 if stance else (cycle - 0.5) * 2.0
    travel = t if stance else smoother_step(t)
    offset = stride * ((0.5 - travel) if stance else (travel - 0.5))
    radians = math.radians(((angle_degrees + 180.0) % 360.0) - 180.0)
    weights = directional_weights(angle_degrees)
    forward = leg_target("march.forward", phase, left, stride)
    backward = leg_target("march.backward", phase, left, stride)
    backward_amount = backward_technique_weight(angle_degrees)
    pitch = mix(forward["foot"], backward["foot"], backward_amount)
    lift = mix(forward["lift"] - sole_lift(forward["foot"], 0.07),
               backward["lift"] - sole_lift(backward["foot"], 0.08),
               backward_amount)
    lift += sole_lift(pitch, mix(0.07, 0.08, backward_amount)) + directional_sole_offset(angle_degrees, phase, stride)
    return {"stance": stance, "t": t,
            "x": math.sin(radians) * offset, "z": -math.cos(radians) * offset,
            "lift": lift, "foot": pitch,
            "toe": mix(forward["toe"], backward["toe"], backward_amount)}


def directional_body_pose(angle_degrees, phase, stride):
    targets = [directional_target(angle_degrees, phase, left, stride) for left in (True, False)]
    sole_correction = directional_sole_offset(angle_degrees, phase, stride)
    for target in targets:
        target["lift"] -= sole_correction
    weights = directional_weights(angle_degrees)
    rhythm = phase * math.pi * 2.0
    slide_strength = weights[3] - weights[2]
    longitudinal_weight = weights[0] + weights[1]
    yaw = slide_strength * (60.0 + math.sin(rhythm)) + longitudinal_weight * math.sin(rhythm) * 1.4
    transfer = pelvis_transfer_pose(targets, yaw, True)
    drop = transfer["y"] + sole_correction
    roll = -math.cos(rhythm) * 0.38
    yaw_radians = math.radians(yaw)
    return {"pelvis_x": transfer["x"]
            + math.cos(rhythm) * 0.0065 * (1.0 - transfer["weight"]),
            "pelvis_y": drop, "pelvis_z": transfer["z"],
            "ik_pelvis_y": drop, "pelvis_yaw": yaw, "pelvis_roll": roll,
            "spine_yaw": -yaw * mix(0.78, 1.0,
                                      smoother_step(abs(slide_strength) * 2.0)),
            "spine_roll": -roll * 0.92,
            "spine_pitch": math.sin(rhythm + 0.18) * 0.22,
            "spine_lift": -drop * 0.95,
            "spine_x": (-math.cos(yaw_radians) * transfer["x"]
                        + math.sin(yaw_radians) * transfer["z"]),
            "spine_z": (-math.sin(yaw_radians) * transfer["x"]
                        - math.cos(yaw_radians) * transfer["z"]),
            "head_pitch": 10.0 - math.sin(rhythm + 0.18) * 0.12}


def sagittal_ik(target_z, lift, pelvis_drop):
    thigh, shin = 0.39, 0.415
    vertical = 0.805 + pelvis_drop - lift
    distance = clamp(math.hypot(vertical, target_z), 0.08, thigh + shin - 0.001)
    target_angle = math.atan2(-target_z, vertical)
    hip_offset = math.acos(clamp((thigh * thigh + distance * distance - shin * shin)
                                 / (2.0 * thigh * distance), -1.0, 1.0))
    interior = math.acos(clamp((thigh * thigh + shin * shin - distance * distance)
                               / (2.0 * thigh * shin), -1.0, 1.0))
    return math.degrees(target_angle + hip_offset), math.degrees(interior - math.pi)


def spatial_ik_vector(target_x, target_z, vertical):
    thigh, shin = 0.39, 0.415
    plane_vertical = math.hypot(vertical, target_x)
    distance = clamp(math.hypot(plane_vertical, target_z), 0.08, thigh + shin - 0.001)
    target_angle = math.atan2(-target_z, plane_vertical)
    hip_offset = math.acos(clamp((thigh * thigh + distance * distance - shin * shin)
                                 / (2.0 * thigh * distance), -1.0, 1.0))
    interior = math.acos(clamp((thigh * thigh + shin * shin - distance * distance)
                               / (2.0 * thigh * shin), -1.0, 1.0))
    return (math.degrees(target_angle + hip_offset), math.degrees(math.atan2(target_x, vertical)),
            math.degrees(interior - math.pi))


def conditioned_recovery_vertical(target, horizontal_squared, vertical, backward_amount):
    if target["stance"] or target["t"] >= 0.5:
        return vertical
    envelope = math.sin(math.pi * target["t"] / 0.5) ** 2
    base_flex = mix(1.4, 0.7, backward_amount)
    peak_flex = mix(10.0, 8.0, backward_amount)
    knee_flex = math.radians(mix(base_flex, peak_flex, envelope))
    thigh, shin = 0.39, 0.415
    desired_distance_squared = (thigh * thigh + shin * shin
                                + 2.0 * thigh * shin * math.cos(knee_flex))
    if (vertical * vertical + horizontal_squared <= desired_distance_squared
            or desired_distance_squared <= horizontal_squared):
        return vertical
    return math.sqrt(desired_distance_squared - horizontal_squared)


def leg_pose(mode, phase, left, stride, pelvis_drop, pelvis_z=0.0):
    target = leg_target(mode, phase, left, stride)
    if mode in ("march.forward", "march.backward"):
        backward_amount = 1.0 if mode == "march.backward" else 0.0
        target_z = target["z"] - pelvis_z
        vertical = 0.805 + pelvis_drop - target["lift"]
        vertical = conditioned_recovery_vertical(
            target, target_z ** 2, vertical, backward_amount)
        adjusted_lift = 0.805 + pelvis_drop - vertical
        hip, knee = sagittal_ik(target_z, adjusted_lift, pelvis_drop)
        crossing = (1.0 - smoother_step(abs(target["t"] - 0.5) / 0.45)
                    if not target["stance"] else 0.0)
        inward = (16.2 if left else -16.2) * crossing
        return hip, inward, knee, 0.0, target["foot"] - hip - knee, -inward, target["toe"]
    if mode in ("slide.left", "slide.right"):
        roll = math.degrees(math.asin(clamp(target["x"] / 0.805, -0.62, 0.62)))
        bend = -2.0 if target["stance"] else -7.0 * math.sin(math.pi * target["t"])
        return -bend * 0.38, roll, bend, -roll * 0.08, -bend * 0.62, -roll * 0.92, 0.0
    return (0.0,) * 7


def attention_leg_pose(left):
    inward = 4.7 if left else -4.7
    return (0.0, inward, -0.8, 0.0, 0.8, -inward, 0.0)


def directional_leg_pose(angle_degrees, phase, left, stride, body):
    target = directional_target(angle_degrees, phase, left, stride)
    weights = directional_weights(angle_degrees)
    hip_bind_x = -0.105 if left else 0.105
    crossing = ((1.0 - smoother_step(abs(target["t"] - 0.5) / 0.45))
                if not target["stance"] else 0.0)
    crossing_distance = 0.233 + 0.007 * (weights[2] + weights[3])
    crossing_x = (crossing_distance if left else -crossing_distance) * crossing
    slide_strength = weights[3] - weights[2]
    crossing_forward = (-1.0 if left else 1.0) * slide_strength * 0.026 * crossing
    facing_yaw = math.radians(body["pelvis_yaw"])
    world_x = (hip_bind_x + target["x"] + crossing_x
               + math.sin(facing_yaw) * crossing_forward - body["pelvis_x"])
    world_y = 0.075 + target["lift"] - (0.91 + body["pelvis_y"])
    world_z = (target["z"] + math.cos(facing_yaw) * crossing_forward
               - body.get("pelvis_z", 0.0))
    roll, yaw = math.radians(body["pelvis_roll"]), math.radians(body["pelvis_yaw"])
    roll_x = math.cos(roll) * world_x + math.sin(roll) * world_y
    roll_y = -math.sin(roll) * world_x + math.cos(roll) * world_y
    local_x = math.cos(yaw) * roll_x - math.sin(yaw) * world_z - hip_bind_x
    local_z = math.sin(yaw) * roll_x + math.cos(yaw) * world_z
    local_y = roll_y + 0.03
    vertical = -local_y
    reach = 0.39 + 0.415 - 0.001
    horizontal_squared = local_x * local_x + local_z * local_z
    if vertical * vertical + horizontal_squared > reach * reach:
        vertical = math.sqrt(max(0.0, reach * reach - horizontal_squared))
    backward_amount = backward_technique_weight(angle_degrees)
    vertical = conditioned_recovery_vertical(
        target, horizontal_squared, vertical, backward_amount)
    hip_x, hip_z, knee_x = spatial_ik_vector(local_x, local_z, vertical)
    return (hip_x, hip_z, knee_x, 0.0, target["foot"] - hip_x - knee_x,
            -hip_z - body["pelvis_roll"], target["toe"])


def pose_matrices(mode: str, phase: float, stride: float = 0.5715):
    direction_angle = (float(mode.split(".", 1)[1]) if mode.startswith("direction.")
                       else 90.0 if mode == "slide.right"
                       else -90.0 if mode == "slide.left" else None)
    body = (directional_body_pose(direction_angle, phase, stride)
            if direction_angle is not None else body_pose(mode, phase, stride))
    rotations = {
        1: (0.0, body["pelvis_yaw"], body["pelvis_roll"]),
        2: (body["spine_pitch"], body["spine_yaw"] * 0.50, body["spine_roll"] * 0.45),
        3: (-body["spine_pitch"] * 0.62, body["spine_yaw"] * 0.50, body["spine_roll"] * 0.55),
        5: (body["head_pitch"], 0.0, 0.0),
        14: (-4.0, 0.0, 0.0), 18: (-4.0, 0.0, 0.0),
        15: (111.7, -56.8, -63.0), 19: (111.7, 56.8, 63.0),
        16: (72.0, -57.0, -43.8), 20: (68.0, 55.0, 42.1),
        17: (-91.661, 57.177, -56.840), 21: (-86.091, -59.315, 68.063),
    }
    translations = {1: (body["pelvis_x"], body["pelvis_y"], body["pelvis_z"]),
                    2: (body["spine_x"], body["spine_lift"], body["spine_z"])}
    for left, thigh, shin, foot, toe in (
        (True, 6, 7, 8, 9), (False, 10, 11, 12, 13)
    ):
        if mode == "idle":
            pose = attention_leg_pose(left)
        elif direction_angle is not None:
            pose = directional_leg_pose(direction_angle, phase, left, stride, body)
        else:
            pose = leg_pose(mode, phase, left, stride, body["pelvis_y"],
                            body["pelvis_z"])
        hip_x, hip_z, knee_x, knee_z, foot_x, foot_z, toe_x = pose
        rotations[thigh] = (hip_x, 0.0, hip_z)
        rotations[shin] = (knee_x, 0.0, knee_z)
        rotations[foot] = (foot_x, 43.0 if left and mode == "idle"
                           else -43.0 if mode == "idle" else 0.0, foot_z)
        rotations[toe] = (toe_x, 0.0, 0.0)

    globals_ = []
    skins = []
    for index, joint in enumerate(JOINTS):
        if joint.parent is None:
            local_position = joint.global_position
        else:
            parent_position = JOINTS[joint.parent].global_position
            local_position = tuple(joint.global_position[axis] - parent_position[axis] for axis in range(3))
        offset = translations.get(index, (0.0, 0.0, 0.0))
        local_position = tuple(local_position[axis] + offset[axis] for axis in range(3))
        local = multiply(translation(*local_position), rotation(*rotations.get(index, (0.0, 0.0, 0.0))))
        global_matrix = local if joint.parent is None else multiply(globals_[joint.parent], local)
        globals_.append(global_matrix)
        skins.append(multiply(global_matrix, translation(*(-value for value in joint.global_position))))
    return skins


def skinned_vertices(positions, joints, weights, matrices):
    output = []
    for position, joint_row, weight_row in zip(positions, joints, weights):
        result = [0.0, 0.0, 0.0]
        for joint, weight in zip(joint_row, weight_row):
            if weight <= 0.0:
                continue
            moved = transform(matrices[int(joint)], position)
            for axis in range(3):
                result[axis] += moved[axis] * weight
        output.append(tuple(result))
    return output


def color(base, brightness):
    return "#" + "".join(f"{max(0, min(255, int(channel * brightness))):02x}" for channel in base)


def render_bmp(source: pathlib.Path, destination: pathlib.Path):
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])
    width, height, ground = 2160, 720, 630
    pixels = bytearray(width * height * 3)
    for y in range(height):
        base = (40, 91, 61) if y >= ground else (135, 170, 189)
        for x in range(width):
            offset = ((height - 1 - y) * width + x) * 3
            pixels[offset:offset+3] = bytes((base[2], base[1], base[0]))
    for x in range(width):
        offset = ((height - 1 - ground) * width + x) * 3
        pixels[offset:offset+3] = bytes((223, 233, 220))
    poses = [
        ("idle", 0.0, 180, (169, 103, 75), 0.0),
        ("march.forward", 0.50, 540, (217, 163, 127), math.radians(78)),
        ("march.forward", 0.575, 900, (205, 149, 111), math.radians(78)),
        ("march.forward", 0.65, 1260, (190, 128, 91), math.radians(78)),
        ("march.forward", 0.75, 1620, (159, 96, 70), 0.0),
        ("march.backward", 0.50, 1980, (112, 65, 47), math.radians(78)),
    ]
    for mode, phase, center_x, base, view_angle in poses:
        vertices = skinned_vertices(positions, joints, weights, pose_matrices(mode, phase))
        cosine, sine = math.cos(view_angle), math.sin(view_angle)
        viewed = [(x * cosine + z * sine, y, -x * sine + z * cosine) for x, y, z in vertices]
        for x, y, depth in sorted(viewed, key=lambda point: point[2], reverse=True):
            px, py = int(center_x + x * 285.0), int(ground - y * 285.0)
            shade = max(0.58, min(1.08, 0.86 - depth * 0.9))
            rgb = tuple(max(0, min(255, int(channel * shade))) for channel in base)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = px + dx, py + dy
                    if 0 <= xx < width and 0 <= yy < height:
                        offset = ((height - 1 - yy) * width + xx) * 3
                        pixels[offset:offset+3] = bytes((rgb[2], rgb[1], rgb[0]))
    row_bytes = width * 3
    padding = (4 - row_bytes % 4) % 4
    pixel_bytes = b"".join(pixels[row*row_bytes:(row+1)*row_bytes] + b"\0" * padding for row in range(height))
    file_size = 54 + len(pixel_bytes)
    header = (b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
              + struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0,
                            len(pixel_bytes), 2835, 2835, 0, 0))
    destination.write_bytes(header + pixel_bytes)
    print(destination)


def render(source: pathlib.Path, destination: pathlib.Path):
    if destination.suffix.lower() == ".bmp":
        render_bmp(source, destination)
        return
    document, binary = read_glb(source)
    primitive = document["meshes"][0]["primitives"][0]
    attributes = primitive["attributes"]
    positions = read_accessor(document, binary, attributes["POSITION"])
    joints = read_accessor(document, binary, attributes["JOINTS_0"])
    weights = read_accessor(document, binary, attributes["WEIGHTS_0"])
    indices = [row[0] for row in read_accessor(document, binary, primitive["indices"])]

    width, height, ground = 1200, 720, 630
    poses = [
        ("Forward 8-to-5", "march.forward", 0.08, 250, (217, 163, 127)),
        ("Attention", "idle", 0.0, 600, (169, 103, 75)),
        ("Right slide", "slide.right", 0.08, 950, (112, 65, 47)),
    ]
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
           '<rect width="100%" height="100%" fill="#87aabd"/>',
           f'<rect y="{ground}" width="100%" height="{height-ground}" fill="#285b3d"/>',
           f'<line y1="{ground}" y2="{ground}" x2="{width}" stroke="#dce9df" stroke-width="3"/>']

    for label, mode, phase, center_x, base in poses:
        vertices = skinned_vertices(positions, joints, weights, pose_matrices(mode, phase))
        projected = [(center_x + x * 285.0, ground - y * 285.0, z) for x, y, z in vertices]
        triangles = []
        for offset in range(0, len(indices), 3):
            a, b, c = (vertices[indices[offset + item]] for item in range(3))
            ab = (b[0]-a[0], b[1]-a[1], b[2]-a[2])
            ac = (c[0]-a[0], c[1]-a[1], c[2]-a[2])
            normal = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
            if normal[2] >= 0.0:  # front view looks from -Z toward +Z
                continue
            length = math.sqrt(sum(value*value for value in normal)) or 1.0
            diffuse = max(0.0, (-normal[0]*0.25 + normal[1]*0.55 - normal[2]*0.79) / length)
            shade = 0.48 + diffuse * 0.52
            points = [projected[indices[offset + item]] for item in range(3)]
            triangles.append((sum(point[2] for point in points) / 3.0, points, shade))
        triangles.sort(reverse=True, key=lambda item: item[0])
        for _, points, shade in triangles:
            coordinates = " ".join(f"{point[0]:.1f},{point[1]:.1f}" for point in points)
            svg.append(f'<polygon points="{coordinates}" fill="{color(base, shade)}"/>')
        svg.append(f'<text x="{center_x}" y="690" fill="#f7fbff" font-family="sans-serif" font-size="22" text-anchor="middle">{html.escape(label)}</text>')
    svg.append("</svg>")
    destination.write_text("\n".join(svg), encoding="utf-8")
    print(destination)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    arguments = parser.parse_args()
    render(arguments.source, arguments.destination)


if __name__ == "__main__":
    main()
