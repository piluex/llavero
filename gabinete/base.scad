//Metric units
//Definitions
interior_width = 20;
interior_depth = 70;
interior_height = 6;
wall_thickness = 1.1;
usb_width = 12.1;
usb_height = 4.5;
usb_depth = 5;//This is only used as place_holder for the plug
push_width = 6;
push_height = 6;
push_depth = 3;
led_diameter = 3;
led_height = 3;//Only used for placeholder, should be > wall_thickness
safe_distance = 0.05;
push_holder_w = 2.5;
push_holder_h = 1;
push_holder_d = 1;

base_wall_height = interior_height/2;
exterior_width = interior_width + wall_thickness*2;
exterior_depth = interior_depth + wall_thickness*2;
exterior_height = interior_height + wall_thickness*2;

base_height = wall_thickness + base_wall_height;
base_width = exterior_width;
base_depth = exterior_depth;

push_x_pos = exterior_width/2 + (exterior_width/2-push_width)/2;
push_y_pos = base_depth-push_depth+safe_distance;
push_z_pos = (exterior_height-push_height)/2;

//http://www.thingiverse.com/thing:9347 fixed for dimensional precision
module roundedRect(size, radius)
{
	x = size[0];
	y = size[1];
	z = size[2];
	linear_extrude(height=z)
	hull()
	{
		// place 4 circles in the corners, with the given radius
		translate([(radius), (radius), 0])
		circle(r=radius);
	
		translate([(x)-(radius), (radius), 0])
		circle(r=radius);
	
		translate([(radius), (y)-(radius), 0])
		circle(r=radius);
	
		translate([(x)-(radius), (y)-(radius), 0])
		circle(r=radius);
	}
}

module usb_plug_placeholder()
{
    cube([usb_width, usb_depth, usb_height], center = false);
}

module led_placeholder()
{
    rotate([-90,0,0])
        cylinder(led_height, d=led_diameter, center=true,$fn=50);
}

module push_place_holder()
{
    cube([push_width, push_depth, push_height]);
}


module base()
{
    difference()
    {
        roundedRect([base_width, base_depth, base_height], 1,$fn=20);
        translate([wall_thickness,wall_thickness,wall_thickness])
            cube([interior_width,interior_depth,interior_height],center = false);
        translate([(base_width-usb_width)/2,-safe_distance,wall_thickness])
            usb_plug_placeholder();
        
        led_x_center = (exterior_width/4);
        led_y_center = base_depth-wall_thickness-safe_distance;
        led_z_center = exterior_height/2;
        translate([led_x_center, led_y_center, led_z_center])
            led_placeholder();
        
        translate([push_x_pos, push_y_pos, push_z_pos])
            push_place_holder();     
    };
    translate([push_x_pos + (push_width-push_holder_w)/2,push_y_pos-push_holder_d,push_z_pos])
        cube([push_holder_w,push_holder_d,push_holder_h]);
}

base();