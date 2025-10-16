// RPi02W-PeanutGB Enclosure



module radiusedblock(xlen,ylen,zlen,radius) {
	hull() {
		translate([radius,radius,radius]) sphere(r=radius);
		translate([xlen + radius , radius , radius]) sphere(r=radius);
		translate([radius , ylen + radius , radius]) sphere(r=radius);    
		translate([xlen + radius , ylen + radius , radius]) sphere(r=radius);
		translate([radius , radius , zlen + radius]) sphere(r=radius);
		translate([xlen + radius , radius , zlen + radius]) sphere(r=radius);
		translate([radius,ylen + radius,zlen + radius]) sphere(r=radius);
		translate([xlen + radius,ylen + radius,zlen + radius]) sphere(r=radius);
	}
}

$fn = 20; // higher detail to curves

thickness = 2.0;
gap = 0.5;
height = 15.0;
board = 5.0; // top of pcb's 

scale([1, -1, 1]) {
difference() {
union() {
	translate([15.0-gap-thickness,0.0-gap-thickness,0.0-gap-thickness])
	{
		//cube([70.0+2*gap+2*thickness,
		//	57.5+2*gap+2*thickness,
		//	height+2*gap+2*thickness]);
		radiusedblock(70.0+2*gap+2*thickness-5.0,
			55.5+2*gap+2*thickness-5.0,
			height+2*gap+2*thickness-5.0,
			2.5);
	}
	translate([0.0-gap-thickness,55.5-gap-thickness-2.0,0.0-gap-thickness])
	{
		//cube([100.0+2*gap+2*thickness,
		//	62.5+2*gap+2*thickness+2.0,
		//	height+2*gap+2*thickness]);
		radiusedblock(100.0+2*gap+2*thickness-5.0,
			64.5+2*gap+2*thickness+2.0-5.0,
			height+2*gap+2*thickness-5.0,
			2.5);
	}
}
union() {
	translate([15.0-gap,0.0-gap,0.0-gap])
	{
		cube([70.0+2*gap,55.5+2*gap,height+2*gap]);
	}
	translate([0.0-gap,55.5-gap,0.0-gap])
	{
		cube([100.0+2*gap,64.5+2*gap,height+2*gap]);
	}
} 
union() {
	// backlight pot
	translate([23.0,0.0-thickness-2,board+6.5]) {
	rotate([-90,0,0]) {
		cylinder(thickness+4, 3.5, 3.5);
	} }
	// audio jack
	translate([37.5,0.0-thickness-2,board+3.5]) {
	rotate([-90,0,0]) {
		cylinder(thickness+4, 3.5, 3.5);
	} }
	// power switch
	translate([46.0,0.0-thickness-2,board]) {
		cube([8.0,0.0+thickness+4,5.0]);
	}
	// usb port
	translate([57.5,0.0-thickness-2,board]) {
		cube([10.0,0.0+thickness+04,4.0]);
	}
	// audio pot
	translate([77.0,0.0-thickness-2,board+6.5]) {
	rotate([-90,0,0]) {
		cylinder(thickness+4, 3.5, 3.5);
	} }
	// left shoulder button
	translate([5.25,55.5,board+3.0]) {
		translate([-5.75,-thickness-gap-3,-4.0]) {
			cube([10.0,thickness+gap+4,8.0]);
		}
	}
	// right shoulder button
	translate([94.75,55.5,board+3.0]) {
		translate([-4.25,-thickness-gap-3,-4.0]) {
			cube([10.0,thickness+gap+4,8.0]);
		}
	}
	// lcd screen
	translate([23.0,18.0,height-1]) {
		cube([54.0,38.0,thickness+2]);
	}
	// pi ports
	translate([35.0,122.5-gap-thickness,board+2-5.0]) {
		cube([65.0,thickness+2*gap+2*thickness,5.0]);
	}
	// pi sdcard
	translate([100.0-1.0,100.0,board+2-3.0]) {
		cube([thickness+gap+2.0,12.5,3.0]);
		translate([6.75,6.25,1.5]) {
			scale([1,1.85,1]) {
				sphere(5.0);
			}
		}
	}
	// directional buttons
	translate([12.75,69.5,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([12.75,85.0,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([5.0,77.25,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([20.5,77.25,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	// action buttons
	translate([87.25,69.5,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([87.25,85.0,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([79.5,77.25,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([95.0,77.25,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	// start/select buttons
	translate([42.25,77.5,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	translate([57.75,77.5,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	// menu button
	translate([50.0,69.75,height-1]) {
		cylinder(thickness+2,4.0,4.0);
	}
	// led
	translate([68.75,62.5,height-1]) {
		cylinder(thickness+2,3.25,3.25);
	}
	// speaker holes
	translate([7.5, 112.5, height-1]) {
		cylinder(thickness+2, 2, 2);
		translate([0, -6, 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([0, 6, 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([-6, 0, 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([6, 0, 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([-6/sqrt(2), -6/sqrt(2), 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([6/sqrt(2), -6/sqrt(2), 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([6/sqrt(2), 6/sqrt(2), 0]) {
			cylinder(thickness+2, 2, 2);
		}
		translate([-6/sqrt(2), 6/sqrt(2), 0]) {
			cylinder(thickness+2, 2, 2);
		}
	}
	// screw holes
	translate([19.5,20.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	translate([80.75,20.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	translate([19.5,54.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	translate([80.75,54.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	translate([3.5,96.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	//translate([38.5,96.5,-gap-thickness]) {
	//	cylinder(gap+thickness,3.5,1.5);
	//}
	translate([96.5,96.5,-gap-thickness]) {
		cylinder(gap+thickness,3.5,1.5);
	}
	// ventilation holes
	//translate([43.5,100.0,-thickness-gap-1]) {
	//	cylinder(gap+thickness+1,2.0,2.0);
	//	translate([0.0,8.0,0.0]) {
	//		cylinder(gap+thickness+1,2.0,2.0);
	//	}
	//	translate([0.0,16.0,0.0]) {
	//		cylinder(gap+thickness+1,2.0,2.0);
	//	}
	//}
	translate([43.5+8.0,100.0,-thickness-gap-1]) {
		cylinder(gap+thickness+1,2.0,2.0);
		translate([0.0,8.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
		translate([0.0,16.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
	}
	translate([43.5+16.0,100.0,-thickness-gap-1]) {
		cylinder(gap+thickness+1,2.0,2.0);
		translate([0.0,8.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
		translate([0.0,16.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
	}
	translate([43.5+24.0,100.0,-thickness-gap-1]) {
		cylinder(gap+thickness+1,2.0,2.0);
		translate([0.0,8.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
		translate([0.0,16.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
	}
	translate([43.5+32.0,100.0,-thickness-gap-1]) {
		cylinder(gap+thickness+1,2.0,2.0);
		translate([0.0,8.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
		translate([0.0,16.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
	}
	translate([43.5+40.0,100.0,-thickness-gap-1]) {
		cylinder(gap+thickness+1,2.0,2.0);
		translate([0.0,8.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
		translate([0.0,16.0,0.0]) {
			cylinder(gap+thickness+1,2.0,2.0);
		}
	}
	//translate([43.5+48.0,100.0,-thickness-gap-1]) {
	//	cylinder(gap+thickness+1,2.0,2.0);
	//	translate([0.0,8.0,0.0]) {
	//		cylinder(gap+thickness+1,2.0,2.0);
	//	}
	//	translate([0.0,16.0,0.0]) {
	//		cylinder(gap+thickness+1,2.0,2.0);
	//	}
	//}
}

// uncomment to show top half
translate([-10.0,-10.0,-100.0]) {
	//cube([120.0,210.0,100.0+board]);
}
// uncomment to show bottom half
translate([-10.0,-10.0,board]) {
	//cube([120.0,210.0,100.0]);
}

} }


scale([1,-1,1]) {
difference() {
union() {
	// screw supports
	translate([19.5,20.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([-4.75-gap,-2.0,0.0]) {
					cube([4.75+gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	translate([80.75,20.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([0.0,-2.0,0.0]) {
					cube([4.75+gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	translate([19.5,54.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([-6.25-gap,-2.0,0.0]) {
					cube([6.25+2*gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	translate([80.75,54.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([0.0-gap,-2.0,0.0]) {
					cube([6.25+2*gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	translate([3.5,96.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([-4.75-gap,-2.0,0.0]) {
					cube([4.75+gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	//translate([38.5,96.5,0.0-gap]) {
	//	difference() {
	//		union() {
	//			cylinder(height+2*gap,3.0,3.0);
	//			//translate([0.0,-2.0,0.0]) {
	//			//	cube([4.75+gap,4.0,height+2*gap]);
	//			//}
	//		}
	//		cylinder(height+2*gap,1.5,1.5);
	//	}
	//}
	translate([96.5,96.5,0.0-gap]) {
		difference() {
			union() {
				cylinder(height+2*gap,3.0,3.0);
				translate([0.0,-2.0,0.0]) {
					cube([4.75+gap,4.0,height+2*gap]);
				}
			}
			cylinder(height+2*gap,1.5,1.5);
		}
	}
	// directional button supports
	translate([12.75,69.5,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([12.75,85.0,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([5.0,77.25,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([20.5,77.25,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	// action button supports
	translate([87.25,69.5,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([87.25,85.0,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([79.5,77.25,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([95.0,77.25,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	// start/select button supports
	translate([42.25,77.5,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	translate([57.75,77.5,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	// menu button support
	translate([50.0,69.75,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,4.0,4.0);
		}
	}
	// led support
	translate([68.75,62.5,board+6.0]) {
		difference() {
			cylinder(height+thickness+gap-board-6.0-1,6.0,6.0);
			cylinder(height+thickness+gap-board-6.0-1,3.25,3.25);
		}
	}
	// lcd 'supports'
	difference() {
		translate([23.0-2.0,18.0-2.0,board+6.0]) {
			cube([54.0+4.0,38.0+4.0,height+thickness+gap-board-6.0-1]);
		}
		translate([23.0,18.0,board+6.0]) {
			cube([54.0,38.0,height+thickness+gap-board-6.0-1]);
		}
	}
} 
union() {
	translate([15.0-gap,0.0-gap,board-1.6])
	{
		cube([70.0+2*gap,57.5+2*gap,1.6]);
	}
	translate([0.0-gap,57.5-gap,board-1.6])
	{
		cube([100.0+2*gap,63.75+2*gap,1.6]);
	}
} 

// uncomment to show top half
translate([-10.0,-10.0,-100.0]) {
	//cube([120.0,210.0,100.0+board]);
}
// uncomment to show bottom half
translate([-10.0,-10.0,board]) {
	//cube([120.0,210.0,100.0]);
}

} }






module button()
{
	difference()
	{
		union()
		{
			cylinder(height - board - 5 + thickness + gap, 3.5, 3.5);
			cylinder(1.5, 5, 5);
			intersection()
			{
				translate([0, 0, -3])
				{
					sphere(height - board - 5 + thickness + gap + 5);
				}
				cylinder(100, 3.5, 3.5);
			}
		}
		translate([-1, -1, 0])
		{
			cube([2, 2, 4]);
		}
	}
}

module inverse()
{
	difference()
	{
		union()
		{
			cylinder(height - board - 5 + thickness + gap + 2, 3.5, 3.5);
			cylinder(1.5, 5, 5);
		}
		union()
		{
			translate([0, 0, height - board - 5 + thickness + gap + 2 + 9.5])
			{
				sphere(12);
			}
		}
		translate([-1, -1, 0])
		{
			cube([2, 2, 4]);
		}
	}
}

module shoulder()
{
	translate([-3.75, 0, -4.5]) {
		scale([1, -1, 1])
		{
			difference()
			{
				union()
				{
					cube([9, 6, 7]);
					translate([-2, 0, -1])
					{
						cube([11, 2, 9]);
					}
					intersection()
					{
						translate([4.5, 0, 3.5])
						{
							sphere(9);
						}
						cube([9, 100, 7]);
					}
				}
				translate([2.75, 0, 2.5])
				{
					cube([2, 4, 2]);
				}
			}
		}
	}
}

scale([1,-1,1]) {

	translate([12.75,69.5,board+5]) { button(); }
	translate([12.75,85.0,board+5]) { button(); }
	translate([5.0,77.25,board+5]) { button(); }
	translate([20.5,77.25,board+5]) { button(); }
	
	translate([87.25,69.5,board+5]) { button(); }
	translate([87.25,85.0,board+5]) { button(); }
	translate([79.5,77.25,board+5]) { button(); }
	translate([95.0,77.25,board+5]) { button(); }
	
	translate([50.0,69.5,board+5]) { inverse(); }
	translate([42.25,77.25,board+5]) { button(); }
	translate([57.75,77.25,board+5]) { button(); }
	
	translate([5.25, 57.5, board+4]) {
		scale([-1,1,1]) {
			shoulder();
		}
	}
	translate([94.75, 57.5, board+4]) {
		scale([1,1,1]) {
			shoulder();
		}
	}
}








