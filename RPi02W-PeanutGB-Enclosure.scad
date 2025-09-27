// RPi02W-PeanutGB Enclosure

width = 180.0;
depth = 77.5;
height = 20.0;
thickness = 2.0;
boards = 7.0; // top of pcb's 

scale([1, -1, 1]){
difference() {
union()	{	
	difference()
	{
		// main body
		union()
		{
			// main box
			translate([thickness, 0, 0])
			{
				cube([width-thickness*2, depth, height]);
			}
			
			// rounded edges
			difference()
			{
				union() {
				translate([thickness, height/4, height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth-height/2, height/4, height/4);
					}
				}
				translate([thickness, height/4, 3*height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth-height/2, height/4, height/4);
					}
				}
				translate([-height/4+thickness, height/4, height/4])
				{
					cube([height/4, depth-height/2, height/2]);
				}
				translate([thickness, height/4, 0])
				{
					cylinder(height, height/4, height/4);
				}
				translate([thickness, depth-height/4, 0])
				{
					cylinder(height, height/4, height/4);
				}}
			
				difference() {
				translate([-height/4, 0, 0])
				{
					cube([height/4+thickness, depth, height]);
				}
				translate([thickness, 0, height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth, height/4, height/4);
					}
				}
				translate([thickness, 0, 3*height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth, height/4, height/4);
					}
				}
				translate([-height/4+thickness, 0, height/4])
				{
					cube([height/4, depth, height/2]);
				}}
			}
			
			translate([width,0,0]) {
			scale([-1,1,1]) {
			
			difference()
			{
				union() {
				translate([thickness, height/4, height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth-height/2, height/4, height/4);
					}
				}
				translate([thickness, height/4, 3*height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth-height/2, height/4, height/4);
					}
				}
				translate([-height/4+thickness, height/4, height/4])
				{
					cube([height/4, depth-height/2, height/2]);
				}
				translate([thickness, height/4, 0])
				{
					cylinder(height, height/4, height/4);
				}
				translate([thickness, depth-height/4, 0])
				{
					cylinder(height, height/4, height/4);
				}}
			
				difference() {
				translate([-height/4, 0, 0])
				{
					cube([height/4+thickness, depth, height]);
				}
				translate([thickness, 0, height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth, height/4, height/4);
					}
				}
				translate([thickness, 0, 3*height/4])
				{
					rotate([-90, 0, 0])
					{
						cylinder(depth, height/4, height/4);
					}
				}
				translate([-height/4+thickness, 0, height/4])
				{
					cube([height/4, depth, height/2]);
				}}
			}
			
			}}
		}
		
		// hollow inside
		translate([thickness, thickness, thickness])
		{
			cube([width-thickness*2, 
				depth-thickness*2, 
				height-thickness*2]);
		}
		
		// screw holes
		translate([17.5, 10.0, -1])
		{
			cylinder(thickness+0, 4, 1);
			cylinder(thickness+2, 1, 1);
		}
		translate([width-17.5, 10.0, -1])
		{
			cylinder(thickness+0, 4, 1);
			cylinder(thickness+2, 1, 1);
		}
		translate([46.25, 58.75, -1])
		{
			cylinder(thickness+0, 4, 1);
			cylinder(thickness+2, 1, 1);
		}
		translate([width-46.25, 58.75, -1])
		{
			cylinder(thickness+0, 4, 1);
			cylinder(thickness+2, 1, 1);
		}
		
		// backlight pot
		translate([width-8.9, 63.75, -1])
		{
			cylinder(thickness+1, 2.5, 2.5);
		}
		
		// LCD screen
		translate([width/2-35.0, 12.75, height-thickness-1])
		{
			cube([70.0, 50.0, thickness+1]);
		}
		
		// LED
		translate([width-31.25, 13.75, height-thickness-1])
		{
			cylinder(thickness+1, 3, 3);
		}
		
		// speaker holes
		translate([17.5, 57.5, height-thickness-1])
		{
			cylinder(thickness+1, 2, 2);
			translate([0, -7, 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([0, 7, 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([-7, 0, 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([7, 0, 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			
			translate([-7/sqrt(2), -7/sqrt(2), 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([7/sqrt(2), -7/sqrt(2), 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([7/sqrt(2), 7/sqrt(2), 0])
			{
				cylinder(thickness+1, 2, 2);
			}
			translate([-7/sqrt(2), 7/sqrt(2), 0])
			{
				cylinder(thickness+1, 2, 2);
			}
		}
		
		// up/down/left/right buttons
		translate([15.5, 19.5, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([15.5, 35.0, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([7.75, 27.25, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([23.25, 27.25, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		
		// menu button
		translate([45.0, 67.5, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		
		// A/B/X/Y buttons
		translate([width-15.5, 19.5, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([width-15.5, 35.0, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([width-7.75, 27.25, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([width-23.25, 27.25, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		
		// select/start buttons
		translate([width-45.0, 67.5, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		translate([width-34.0, 67.5, height-thickness-1])
		{
			cylinder(thickness+1, 4, 4);
		}
		
		// L/R shoulder buttons
		translate([7.5-4, -1, boards])
		{
			cube([8, thickness+1, 8]);
		}
		translate([width-7.5-4, -1, boards])
		{
			cube([8, thickness+1, 8]);
		}
		
		// audio jack
		translate([28.7, -1, boards+3])
		{
			rotate([-90, 0, 0])
			{
				cylinder(thickness+1, 4, 4);
			}
		}
		
		// audio pot
		translate([43.75-7.5, -1, boards])
		{
			cube([15.0, thickness+1, 3.0]);
			translate([0, thickness, -2])
			{
				cube([15.0, 1, 2.0]);
			}
		}
		
		// usb port
		translate([width-31.25-5.0, -1, boards])
		{
			cube([10.0, thickness+1, 4]);
		}
		
		// power switch
		translate([width-45.0-5.0, -1, boards])
		{
			cube([10.0, thickness+1, 6]); // check size
		}
		
		// raspberry pi zero 2w
		translate([width/2-65.0/2, -1, boards-5.0])
		{
			cube([65.0, thickness+1, 5.0]);
		}
		
		// bottom vents
		translate([width/2-16, thickness+6.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16, thickness+12.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16, thickness+18.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}	
		translate([width/2-16+8.0, thickness+6.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+8.0, thickness+12.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+8.0, thickness+18.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+16.0, thickness+6.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+16.0, thickness+12.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+16.0, thickness+18.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+24.0, thickness+6.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+24.0, thickness+12.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+24.0, thickness+18.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+32.0, thickness+6.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+32.0, thickness+12.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
		translate([width/2-16+32.0, thickness+18.0, -1])
		{
			cylinder(thickness+2, 2, 2);
		}
	}
	
	difference()
	{
		union()
		{
			// screw supports
			translate([17.5, 10.0, thickness])
			{
				difference()
				{
					cylinder(height-thickness, 4, 4);
					cylinder(height-thickness, 1, 1);
				}
			}
			translate([width-17.5, 10.0, thickness])
			{
				difference()
				{
					cylinder(height-thickness, 4, 4);
					cylinder(height-thickness, 1, 1);
				}
			}
			translate([46.25, 58.75, thickness])
			{
				difference()
				{
					cylinder(height-thickness, 4, 4);
					cylinder(height-thickness, 1, 1);
				}
			}
			translate([width-46.25, 58.75, thickness])
			{
				difference()
				{
					cylinder(height-thickness, 4, 4);
					cylinder(height-thickness, 1, 1);
				}
			}
			
			// LED support
			translate([width-31.25, 13.75, boards+5])
			{
				difference()
				{
					cylinder(height-boards-5, 4, 4);
					cylinder(height-boards-5, 3, 3);
				}
			}
			
			// button supports
			translate([15.5, 19.5, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([15.5, 35.0, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([7.75, 27.25, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([23.25, 27.25, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-15.5, 19.5, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-15.5, 35.0, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-7.75, 27.25, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-23.25, 27.25, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([45.0, 67.5, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-45.0, 67.5, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([width-34.0, 67.5, boards+7])
			{
				difference()
				{
					cylinder(height-boards-7, 6, 6);
					cylinder(height-boards-7, 4, 4);
				}
			}
			translate([7.5-4, 0, boards])
			{
				difference()
				{
					translate([-2,0,-boards])
					{
						cube([12, 4, height]);
					}
					cube([8, 4, 8]);
				}
			}
			translate([width-7.5-4, 0, boards])
			{
				difference()
				{
					translate([-2,0,-boards])
					{
						cube([12, 4, height]);
					}
					cube([8, 4, 8]);
				}
			}
		}
		
		// pcb
		difference()
		{
			translate([thickness, thickness, boards-1.6])
			{
				cube([width-thickness*2, 
					depth-thickness*2, 
					1.6]);
			}
			translate([7.5-4, 0, boards])
			{
				translate([-2,0,-2])
				{
					cube([12, 6, 2]);
				}
			}
			translate([width-7.5-4, 0, boards])
			{
				translate([-2,0,-2])
				{
					cube([12, 6, 2]);
				}
			}
		}
	}
} 

union() {
	// uncomment to remove top
	translate([-10, -10, boards])
	{
		//cube([width+20, depth+20, height-boards+10]);
	}

	// uncomment to remove bottom
	translate([-10, -10, -10])
	{
		//cube([width+20, depth+20, boards+10]);
	}
} } }

module button(x,y,z)
{
	translate([x, y, z])
	{
		difference()
		{
			union()
			{
				cylinder(height - boards - 5, 3.5, 3.5);
				cylinder(1.5, 5, 5);
				intersection()
				{
					translate([0, 0, -5])
					{
						sphere(height - boards - 5 + 5 + 2);
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
}

module shoulder(x,y,z)
{
	translate([x-3.5, y, z-3.5])
	{
		scale([1,-1,1])
		{
			difference()
			{
				union()
				{
					cube([7, 6, 7]);
					translate([-1,0,-1])
					{
						cube([9,1.5,9]);
					}
					intersection()
					{
						translate([3.5, 0, 3.5])
						{
							sphere(8);
						}
						cube([7, 100, 7]);
					}
				}
				translate([3, 0, 3])
				{
					cube([2, 4, 2]);
				}
			}
		}
	}
}

scale([1,-1,1]) {
	button(15.5, 19.5, boards+5);
	button(15.5, 35.0, boards+5);
	button(7.75, 27.25, boards+5);
	button(23.25, 27.25, boards+5);
	button(width-15.5, 19.5, boards+5);
	button(width-15.5, 35.0, boards+5);
	button(width-7.75, 27.25, boards+5);
	button(width-23.25, 27.25, boards+5);
	button(45.0, 67.5, boards+5);
	button(width-45.0, 67.5, boards+5);
	button(width-34.0, 67.5, boards+5);

	shoulder(7.5, 6.0, boards+4);
	shoulder(width-7.5, 6.0, boards+4);
}









