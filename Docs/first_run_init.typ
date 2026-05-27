#align(center)[
  = Handling Errata in PCB Design
  #v(1.5em)
]

#let Impact = [
  #text(fill: red, weight: "extrabold")[
    Impact:
  ]
]

#let Solution = [
  #text(fill: blue, weight: "extrabold")[
    Solution:
  ]
]

+ We have *vias underneath the Type-C port*, which aren't filled as it would add to the BoM.
  Add to that we've Grounded the mounting pins to try and protect against ESD. This will have
  the unintended side-effect of shorting the *USB_D* nets to *GND* via the *Type-C's mounting
  case.* #v(0.1em)
  - #Impact The Micro Controller *NOT* being visible as a *JTAG interface on HOST/PC*. Worst case
    damage the HOST/PC's USB Port if it's not protected.
  - #Solution Check the continuity between the *Type-C* mounting case, and the *0 $Omega$*
    resistors. *Rectify before first boot* if a short is indeed present.
