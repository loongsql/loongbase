# +--------------------------------------------------------------------+
# |                         Testing limits                             |
# +--------------------------------------------------------------------+
# |                       Falcon_limit1.test                           |
# |                    Copyright 2006 by MySQL AB                      |
# +--------------------------------------------------------------------+
# | Georg Richter, Hakan Kuecuekyilmaz                                 |
# +--------------------------------------------------------------------+
--disable_query_log
--disable_warnings
DROP TABLE IF EXISTS T001_1;
DROP TABLE IF EXISTS T001_2;
DROP TABLE IF EXISTS T001_3;
DROP VIEW IF EXISTS V001_1;
DROP VIEW IF EXISTS V001_2;
--enable_warnings
--enable_query_log

--echo ***************** changjiang_limit1 *****************
#
# T001_01
# Max. columns per table. We require 749 columns
#
CREATE TABLE T001_1 (
C0001 VARCHAR(4), C0002 INT, C0003 DOUBLE, C0004 TINYINT, C0005 INT,
   C0006 TINYINT, C0007 VARBINARY(9), C0008 DOUBLE, C0009 CHAR(1),
   C0010 VARBINARY(4), C0011 VARBINARY(10), C0012 DOUBLE, C0013 TINYINT,
   C0014 INT, C0015 TINYINT, C0016 DOUBLE, C0017 TINYINT, C0018 DOUBLE,
   C0019 INT, C0020 VARCHAR(2), C0021 INT, C0022 TINYINT, C0023 VARBINARY(2),
   C0024 DOUBLE, C0025 TINYINT, C0026 VARCHAR(9), C0027 VARCHAR(5),
   C0028 DOUBLE, C0029 VARCHAR(1), C0030 DOUBLE, C0031 DOUBLE, C0032 VARBINARY(2),
   C0033 VARBINARY(8), C0034 CHAR(8), C0035 DOUBLE, C0036 DOUBLE,
   C0037 VARBINARY(3), C0038 DOUBLE, C0039 CHAR(8), C0040 VARBINARY(7),
   C0041 CHAR(3), C0042 CHAR(1), C0043 VARBINARY(4), C0044 DOUBLE,
   C0045 CHAR(9), C0046 TINYINT, C0047 VARCHAR(10), C0048 CHAR(4),
   C0049 VARCHAR(8), C0050 INT, C0051 VARBINARY(10), C0052 VARCHAR(6),
   C0053 TINYINT, C0054 INT, C0055 TINYINT, C0056 TINYINT, C0057 DOUBLE,
   C0058 VARCHAR(7), C0059 INT, C0060 DOUBLE, C0061 CHAR(2), C0062 VARBINARY(1),
   C0063 INT, C0064 CHAR(8), C0065 INT, C0066 CHAR(8), C0067 CHAR(1),
   C0068 TINYINT, C0069 VARCHAR(4), C0070 TINYINT, C0071 INT, C0072 CHAR(10),
   C0073 INT, C0074 VARBINARY(2), C0075 VARBINARY(7), C0076 VARCHAR(2),
   C0077 VARBINARY(5), C0078 TINYINT, C0079 VARBINARY(7), C0080 VARCHAR(8),
   C0081 INT, C0082 VARCHAR(6), C0083 INT, C0084 TINYINT, C0085 INT,
   C0086 CHAR(2), C0087 DOUBLE, C0088 TINYINT, C0089 DOUBLE, C0090 INT,
   C0091 VARCHAR(7), C0092 VARBINARY(3), C0093 INT, C0094 TINYINT,
   C0095 VARBINARY(9), C0096 VARCHAR(9), C0097 DOUBLE, C0098 CHAR(10),
   C0099 DOUBLE, C0100 VARCHAR(1), C0101 DOUBLE, C0102 TINYINT,
   C0103 VARBINARY(5), C0104 VARBINARY(6), C0105 TINYINT, C0106 INT,
   C0107 VARCHAR(9), C0108 INT, C0109 DOUBLE, C0110 CHAR(6), C0111 INT,
   C0112 CHAR(10), C0113 INT, C0114 DOUBLE, C0115 TINYINT, C0116 DOUBLE,
   C0117 TINYINT, C0118 INT, C0119 VARCHAR(9), C0120 VARCHAR(9),
   C0121 VARBINARY(7), C0122 VARCHAR(3), C0123 VARCHAR(8), C0124 DOUBLE,
   C0125 DOUBLE, C0126 DOUBLE, C0127 TINYINT, C0128 DOUBLE, C0129 INT,
   C0130 TINYINT, C0131 TINYINT, C0132 VARBINARY(6), C0133 CHAR(8),
   C0134 VARCHAR(1), C0135 VARBINARY(1), C0136 DOUBLE, C0137 VARBINARY(6),
   C0138 VARCHAR(4), C0139 TINYINT, C0140 VARBINARY(3), C0141 VARBINARY(8),
   C0142 TINYINT, C0143 INT, C0144 CHAR(2), C0145 INT, C0146 VARCHAR(3),
   C0147 DOUBLE, C0148 VARBINARY(5), C0149 DOUBLE, C0150 CHAR(7),
   C0151 CHAR(5), C0152 INT, C0153 DOUBLE, C0154 CHAR(3), C0155 DOUBLE,
   C0156 CHAR(7), C0157 DOUBLE, C0158 VARCHAR(1), C0159 DOUBLE,
   C0160 TINYINT, C0161 VARCHAR(8), C0162 TINYINT, C0163 DOUBLE,
   C0164 INT, C0165 TINYINT, C0166 VARCHAR(6), C0167 DOUBLE, C0168 VARBINARY(9),
   C0169 INT, C0170 INT, C0171 CHAR(7), C0172 VARBINARY(4), C0173 TINYINT,
   C0174 CHAR(3), C0175 CHAR(6), C0176 TINYINT, C0177 TINYINT, C0178 VARCHAR(3),
   C0179 CHAR(7), C0180 INT, C0181 INT, C0182 TINYINT, C0183 TINYINT,
   C0184 DOUBLE, C0185 INT, C0186 VARCHAR(6), C0187 VARCHAR(4),
   C0188 DOUBLE, C0189 VARBINARY(4), C0190 TINYINT, C0191 DOUBLE,
   C0192 TINYINT, C0193 DOUBLE, C0194 VARCHAR(3), C0195 CHAR(7),
   C0196 VARBINARY(6), C0197 TINYINT, C0198 VARCHAR(6), C0199 DOUBLE,
   C0200 INT, C0201 VARCHAR(6), C0202 VARBINARY(3), C0203 DOUBLE,
   C0204 TINYINT, C0205 INT, C0206 DOUBLE, C0207 VARBINARY(2), C0208 TINYINT,
   C0209 DOUBLE, C0210 DOUBLE, C0211 CHAR(8), C0212 DOUBLE, C0213 VARBINARY(6),
   C0214 TINYINT, C0215 INT, C0216 VARBINARY(6), C0217 DOUBLE, C0218 VARBINARY(8),
   C0219 INT, C0220 CHAR(2), C0221 CHAR(4), C0222 TINYINT, C0223 CHAR(5),
   C0224 DOUBLE, C0225 TINYINT, C0226 VARBINARY(8), C0227 CHAR(8),
   C0228 CHAR(3), C0229 TINYINT, C0230 DOUBLE, C0231 INT, C0232 DOUBLE,
   C0233 TINYINT, C0234 VARBINARY(5), C0235 VARCHAR(2), C0236 VARCHAR(1),
   C0237 VARCHAR(8), C0238 TINYINT, C0239 TINYINT, C0240 CHAR(6),
   C0241 VARBINARY(3), C0242 TINYINT, C0243 INT, C0244 CHAR(4),
   C0245 CHAR(4), C0246 VARCHAR(6), C0247 INT, C0248 CHAR(5), C0249 TINYINT,
   C0250 DOUBLE, C0251 TINYINT, C0252 DOUBLE, C0253 TINYINT, C0254 DOUBLE,
   C0255 INT, C0256 INT, C0257 VARCHAR(5), C0258 INT, C0259 INT,
   C0260 DOUBLE, C0261 DOUBLE, C0262 INT, C0263 TINYINT, C0264 VARCHAR(5),
   C0265 VARBINARY(4), C0266 CHAR(1), C0267 VARBINARY(3), C0268 DOUBLE,
   C0269 CHAR(8), C0270 VARCHAR(6), C0271 INT, C0272 CHAR(9), C0273 INT,
   C0274 CHAR(3), C0275 INT, C0276 DOUBLE, C0277 INT, C0278 INT,
   C0279 VARCHAR(7), C0280 VARBINARY(4), C0281 INT, C0282 CHAR(9),
   C0283 CHAR(3), C0284 DOUBLE, C0285 INT, C0286 CHAR(3), C0287 TINYINT,
   C0288 INT, C0289 TINYINT, C0290 VARCHAR(5), C0291 DOUBLE, C0292 CHAR(2),
   C0293 CHAR(7), C0294 CHAR(2), C0295 TINYINT, C0296 VARCHAR(4),
   C0297 TINYINT, C0298 TINYINT, C0299 INT, C0300 CHAR(3), C0301 VARCHAR(8),
   C0302 CHAR(8), C0303 VARCHAR(6), C0304 INT, C0305 INT, C0306 TINYINT,
   C0307 INT, C0308 INT, C0309 CHAR(6), C0310 VARCHAR(7), C0311 VARCHAR(2),
   C0312 INT, C0313 INT, C0314 CHAR(2), C0315 CHAR(10), C0316 CHAR(10),
   C0317 VARBINARY(9), C0318 VARBINARY(3), C0319 CHAR(9), C0320 INT,
   C0321 CHAR(4), C0322 CHAR(5), C0323 INT, C0324 DOUBLE, C0325 INT,
   C0326 INT, C0327 VARBINARY(2), C0328 VARBINARY(8), C0329 CHAR(2),
   C0330 DOUBLE, C0331 INT, C0332 VARBINARY(8), C0333 TINYINT, C0334 DOUBLE,
   C0335 CHAR(1), C0336 INT, C0337 CHAR(6), C0338 TINYINT, C0339 TINYINT,
   C0340 DOUBLE, C0341 VARBINARY(2), C0342 INT, C0343 TINYINT, C0344 TINYINT,
   C0345 INT, C0346 CHAR(2), C0347 VARCHAR(2), C0348 DOUBLE, C0349 VARBINARY(9),
   C0350 CHAR(1), C0351 DOUBLE, C0352 DOUBLE, C0353 INT, C0354 DOUBLE,
   C0355 CHAR(9), C0356 VARBINARY(2), C0357 DOUBLE, C0358 VARBINARY(2),
   C0359 VARCHAR(1), C0360 DOUBLE, C0361 TINYINT, C0362 VARBINARY(9),
   C0363 CHAR(7), C0364 VARBINARY(2), C0365 CHAR(2), C0366 INT,
   C0367 VARBINARY(1), C0368 VARBINARY(10), C0369 INT, C0370 TINYINT,
   C0371 CHAR(10), C0372 VARCHAR(4), C0373 VARBINARY(4), C0374 VARCHAR(4),
   C0375 CHAR(5), C0376 DOUBLE, C0377 VARBINARY(8), C0378 CHAR(1),
   C0379 TINYINT, C0380 TINYINT, C0381 TINYINT, C0382 TINYINT, C0383 CHAR(10),
   C0384 INT, C0385 VARCHAR(3), C0386 DOUBLE, C0387 TINYINT, C0388 CHAR(10),
   C0389 DOUBLE, C0390 TINYINT, C0391 CHAR(3), C0392 DOUBLE, C0393 INT,
   C0394 TINYINT, C0395 VARCHAR(4), C0396 INT, C0397 VARBINARY(4),
   C0398 DOUBLE, C0399 CHAR(10), C0400 VARCHAR(10), C0401 TINYINT,
   C0402 INT, C0403 INT, C0404 CHAR(1), C0405 VARCHAR(7), C0406 CHAR(1),
   C0407 DOUBLE, C0408 INT, C0409 VARBINARY(4), C0410 TINYINT, C0411 VARBINARY(5),
   C0412 TINYINT, C0413 VARBINARY(2), C0414 VARCHAR(3), C0415 INT,
   C0416 TINYINT, C0417 VARCHAR(3), C0418 DOUBLE, C0419 VARBINARY(3),
   C0420 VARBINARY(4), C0421 DOUBLE, C0422 CHAR(9), C0423 VARCHAR(5),
   C0424 DOUBLE, C0425 VARBINARY(2), C0426 VARCHAR(6), C0427 VARCHAR(8),
   C0428 DOUBLE, C0429 DOUBLE, C0430 VARBINARY(2), C0431 VARCHAR(9),
   C0432 VARCHAR(3), C0433 INT, C0434 DOUBLE, C0435 VARCHAR(8),
   C0436 CHAR(6), C0437 DOUBLE, C0438 CHAR(5), C0439 TINYINT, C0440 VARBINARY(1),
   C0441 CHAR(4), C0442 VARCHAR(2), C0443 INT, C0444 VARCHAR(1),
   C0445 INT, C0446 INT, C0447 INT, C0448 DOUBLE, C0449 INT, C0450 TINYINT,
   C0451 VARCHAR(7), C0452 INT, C0453 CHAR(6), C0454 INT, C0455 DOUBLE,
   C0456 VARCHAR(4), C0457 INT, C0458 CHAR(3), C0459 VARBINARY(1),
   C0460 TINYINT, C0461 CHAR(4), C0462 CHAR(6), C0463 DOUBLE, C0464 VARBINARY(10),
   C0465 TINYINT, C0466 VARCHAR(9), C0467 VARBINARY(8), C0468 CHAR(1),
   C0469 CHAR(6), C0470 VARBINARY(3), C0471 CHAR(3), C0472 VARCHAR(7),
   C0473 CHAR(2), C0474 DOUBLE, C0475 TINYINT, C0476 DOUBLE, C0477 CHAR(4),
   C0478 INT, C0479 VARCHAR(10), C0480 CHAR(9), C0481 INT, C0482 VARCHAR(8),
   C0483 VARBINARY(2), C0484 DOUBLE, C0485 VARBINARY(6), C0486 TINYINT,
   C0487 VARBINARY(10), C0488 VARBINARY(4), C0489 INT, C0490 CHAR(3),
   C0491 VARBINARY(5), C0492 VARBINARY(2), C0493 VARBINARY(9), C0494 CHAR(4),
   C0495 VARBINARY(9), C0496 TINYINT, C0497 VARBINARY(9), C0498 VARCHAR(5),
   C0499 DOUBLE, C0500 VARCHAR(8), C0501 DOUBLE, C0502 DOUBLE, C0503 VARBINARY(7),
   C0504 INT, C0505 TINYINT, C0506 INT, C0507 INT, C0508 INT, C0509 VARBINARY(6),
   C0510 TINYINT, C0511 CHAR(4), C0512 CHAR(2), C0513 TINYINT, C0514 VARCHAR(3),
   C0515 CHAR(1), C0516 DOUBLE, C0517 DOUBLE, C0518 TINYINT, C0519 VARCHAR(4),
   C0520 CHAR(9), C0521 DOUBLE, C0522 VARBINARY(10), C0523 TINYINT,
   C0524 VARBINARY(1), C0525 CHAR(10), C0526 VARCHAR(9), C0527 CHAR(10),
   C0528 INT, C0529 DOUBLE, C0530 DOUBLE, C0531 VARCHAR(9), C0532 VARBINARY(6),
   C0533 VARCHAR(10), C0534 TINYINT, C0535 TINYINT, C0536 DOUBLE,
   C0537 INT, C0538 TINYINT, C0539 VARBINARY(1), C0540 VARCHAR(2),
   C0541 INT, C0542 INT, C0543 INT, C0544 DOUBLE, C0545 VARCHAR(9),
   C0546 TINYINT, C0547 VARCHAR(9), C0548 VARBINARY(3), C0549 VARBINARY(1),
   C0550 CHAR(2), C0551 VARBINARY(4), C0552 CHAR(9), C0553 INT,
   C0554 TINYINT, C0555 VARBINARY(9), C0556 INT, C0557 DOUBLE, C0558 INT,
   C0559 INT, C0560 VARBINARY(8), C0561 DOUBLE, C0562 INT, C0563 DOUBLE,
   C0564 DOUBLE, C0565 DOUBLE, C0566 VARBINARY(6), C0567 VARCHAR(2),
   C0568 CHAR(9), C0569 VARBINARY(8), C0570 VARCHAR(5), C0571 VARCHAR(7),
   C0572 TINYINT, C0573 VARBINARY(3), C0574 CHAR(8), C0575 TINYINT,
   C0576 VARCHAR(3), C0577 INT, C0578 VARBINARY(10), C0579 CHAR(7),
   C0580 VARCHAR(3), C0581 DOUBLE, C0582 DOUBLE, C0583 VARCHAR(3),
   C0584 CHAR(4), C0585 INT, C0586 DOUBLE, C0587 VARCHAR(1), C0588 INT,
   C0589 TINYINT, C0590 INT, C0591 VARCHAR(7), C0592 DOUBLE, C0593 TINYINT,
   C0594 VARCHAR(6), C0595 TINYINT, C0596 DOUBLE, C0597 VARBINARY(8),
   C0598 VARBINARY(3), C0599 INT, C0600 VARBINARY(1), C0601 INT,
   C0602 VARCHAR(8), C0603 VARCHAR(7), C0604 INT, C0605 DOUBLE,
   C0606 VARCHAR(6), C0607 VARBINARY(5), C0608 INT, C0609 INT, C0610 VARCHAR(5),
   C0611 INT, C0612 VARCHAR(8), C0613 TINYINT, C0614 DOUBLE, C0615 CHAR(7),
   C0616 TINYINT, C0617 TINYINT, C0618 TINYINT, C0619 CHAR(4), C0620 VARBINARY(5),
   C0621 VARCHAR(4), C0622 DOUBLE, C0623 VARBINARY(2), C0624 TINYINT,
   C0625 DOUBLE, C0626 CHAR(10), C0627 TINYINT, C0628 VARCHAR(5),
   C0629 CHAR(7), C0630 VARBINARY(9), C0631 TINYINT, C0632 CHAR(6),
   C0633 VARBINARY(8), C0634 CHAR(4), C0635 INT, C0636 VARBINARY(7),
   C0637 CHAR(9), C0638 DOUBLE, C0639 DOUBLE, C0640 TINYINT, C0641 DOUBLE,
   C0642 VARBINARY(3), C0643 VARCHAR(5), C0644 VARBINARY(10), C0645 INT,
   C0646 VARBINARY(4), C0647 CHAR(3), C0648 DOUBLE, C0649 DOUBLE,
   C0650 VARCHAR(1), C0651 CHAR(4), C0652 VARBINARY(9), C0653 INT,
   C0654 INT, C0655 VARBINARY(5), C0656 DOUBLE, C0657 INT, C0658 DOUBLE,
   C0659 TINYINT, C0660 VARCHAR(3), C0661 INT, C0662 VARCHAR(6),
   C0663 VARCHAR(6), C0664 VARBINARY(6), C0665 VARBINARY(3), C0666 VARCHAR(1),
   C0667 VARCHAR(4), C0668 DOUBLE, C0669 DOUBLE, C0670 DOUBLE, C0671 VARCHAR(9),
   C0672 TINYINT, C0673 VARCHAR(3), C0674 TINYINT, C0675 DOUBLE,
   C0676 INT, C0677 TINYINT, C0678 VARCHAR(10), C0679 INT, C0680 VARCHAR(10),
   C0681 VARCHAR(2), C0682 INT, C0683 VARCHAR(10), C0684 DOUBLE,
   C0685 INT, C0686 INT, C0687 VARCHAR(4), C0688 INT, C0689 VARCHAR(4),
   C0690 DOUBLE, C0691 TINYINT, C0692 VARCHAR(6), C0693 VARCHAR(3),
   C0694 CHAR(9), C0695 VARCHAR(1), C0696 VARBINARY(1), C0697 TINYINT,
   C0698 TINYINT, C0699 VARBINARY(2), C0700 INT, C0701 DOUBLE, C0702 TINYINT,
   C0703 CHAR(4), C0704 VARBINARY(1), C0705 VARBINARY(8), C0706 VARCHAR(10),
   C0707 VARBINARY(2), C0708 VARCHAR(4), C0709 CHAR(10), C0710 TINYINT,
   C0711 INT, C0712 DOUBLE, C0713 VARCHAR(3), C0714 DOUBLE, C0715 CHAR(1),
   C0716 INT, C0717 TINYINT, C0718 DOUBLE, C0719 VARCHAR(8), C0720 VARCHAR(4),
   C0721 VARCHAR(2), C0722 CHAR(3), C0723 CHAR(9), C0724 VARBINARY(8),
   C0725 DOUBLE, C0726 VARCHAR(9), C0727 VARBINARY(6), C0728 INT,
   C0729 VARBINARY(1), C0730 VARBINARY(3), C0731 INT, C0732 VARCHAR(4),
   C0733 INT, C0734 INT, C0735 CHAR(8), C0736 TINYINT, C0737 DOUBLE,
   C0738 TINYINT, C0739 CHAR(9), C0740 DOUBLE, C0741 VARBINARY(6),
   C0742 TINYINT, C0743 INT, C0744 INT, C0745 CHAR(9), C0746 TINYINT,
   C0747 VARBINARY(8), C0748 DOUBLE, C0749 CHAR(10));

#
# ALTER TABLE TEST (without length increase)
#
ALTER TABLE T001_1
	CHANGE C0009 C0009 TINYINT,
	CHANGE C0042 C0042 TINYINT,
	CHANGE C0067 C0067 TINYINT,
	CHANGE C0266 C0266 TINYINT,
	CHANGE C0335 C0335 TINYINT,
	CHANGE C0350 C0350 TINYINT,
	CHANGE C0378 C0378 TINYINT,
	CHANGE C0404 C0404 TINYINT,
	CHANGE C0406 C0406 TINYINT,
	CHANGE C0468 C0468 TINYINT,
	CHANGE C0515 C0515 TINYINT,
	CHANGE C0715 C0715 TINYINT,
	CHANGE C0004 C0004 CHAR(1),
	CHANGE C0006 C0006 CHAR(1),
	CHANGE C0015 C0015 CHAR(1),
	CHANGE C0017 C0017 CHAR(1),
	CHANGE C0025 C0025 CHAR(1),
	CHANGE C0046 C0046 CHAR(1),
	CHANGE C0053 C0053 CHAR(1),
	CHANGE C0055 C0055 CHAR(1),
	CHANGE C0056 C0056 CHAR(1),
	CHANGE C0070 C0070 CHAR(1),
	CHANGE C0068 C0068 CHAR(1),
	CHANGE C0084 C0084 CHAR(1),
	CHANGE C0088 C0088 CHAR(1),
	CHANGE C0094 C0094 CHAR(1),
	CHANGE C0102 C1002 CHAR(1),
	CHANGE C0105 C1005 CHAR(1),
	CHANGE C0115 C1015 CHAR(1),
	CHANGE C0117 C1017 CHAR(1),
	CHANGE C0127 C1027 CHAR(1),
	CHANGE C0130 C1030 CHAR(1),
	CHANGE C0131 C1031 CHAR(1),
	CHANGE C0139 C1039 CHAR(1),
	CHANGE C0142 C1042 CHAR(1),
	CHANGE C0160 C1060 CHAR(1),
	CHANGE C0162 C1062 CHAR(1),
	CHANGE C0165 C1065 CHAR(1),
	CHANGE C0173 C1073 CHAR(1);
# Varbinary test 
CREATE TABLE T001_2 (
	C001 VARBINARY(8060)
);
# We require at least length 8000 for varchar
CREATE TABLE T001_3 (
	C001 VARCHAR(8060)
);
# Min. 749 columns per VIEW
CREATE VIEW V001_1 AS
	SELECT * FROM T001_1;
# Min. row length for views: 8088
CREATE VIEW V001_2 AS
	SELECT *, '1234567890123456789012345678' FROM T001_1;

# Clean up
DROP VIEW V001_1, V001_2;
DROP TABLE T001_1, T001_2, T001_3;
