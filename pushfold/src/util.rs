pub fn scalar_mul(a: &mut [f32], b: f32) {
    a.iter_mut().for_each(|x| *x *= b);
}

pub fn scalar_div(a: &mut [f32], b: f32) {
    a.iter_mut().for_each(|x| *x /= b);
}

pub fn scalar_sub(a: &mut [f32], b: f32) {
    a.iter_mut().for_each(|x| *x -= b);
}

pub fn elem_mul(a: &mut [f32], b: &[f32]) {
    a.iter_mut().zip(b.iter()).for_each(|(x, y)| *x *= y);
}

pub fn elem_div(a: &mut [f32], b: &[f32]) {
    a.iter_mut().zip(b.iter()).for_each(|(x, y)| *x /= y);
}

pub fn elem_add(a: &mut [f32], b: &[f32]) {
    a.iter_mut().zip(b.iter()).for_each(|(x, y)| *x += y);
}

pub fn elem_sub(a: &mut [f32], b: &[f32]) {
    a.iter_mut().zip(b.iter()).for_each(|(x, y)| *x -= y);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scalar_mul() {
        let mut x = [1.0_f32, 2., 3.];
        scalar_mul(&mut x, 0.5);
        assert_eq!(&x, &[0.5, 1., 1.5]);
    }

    #[test]
    fn test_scalar_div() {
        let mut x = [1.0_f32, 2., 3.];
        scalar_div(&mut x, 2.0);
        assert_eq!(&x, &[0.5, 1., 1.5]);
    }

    #[test]
    fn test_scalar_sub() {
        let mut x = [1.0_f32, 2., 3.];
        scalar_sub(&mut x, 1.0);
        assert_eq!(&x, &[0.0, 1.0, 2.0]);
    }

    #[test]
    fn test_inplace_emul() {
        let mut x = [1.0_f32, 2., 3.];
        elem_mul(&mut x, &[4., 5., 6.]);
        assert_eq!(&x, &[4.0, 10., 18.]);
    }

    #[test]
    fn test_inplace_ediv() {
        let mut x = [1.0_f32, 2., 3.];
        elem_div(&mut x, &[4., 5., 6.]);
        assert_eq!(&x, &[0.25, 0.4, 0.5]);
    }

    #[test]
    fn test_inplace_add() {
        let mut x = [1.0_f32, 2., 3.];
        elem_add(&mut x, &[4., 5., 6.]);
        assert_eq!(&x, &[5.0, 7.0, 9.0]);
    }

    #[test]
    fn test_inplace_sub() {
        let mut x = [1.0_f32, 2., 3.];
        elem_sub(&mut x, &[4., 5., 6.]);
        assert_eq!(&x, &[-3.0, -3.0, -3.0]);
    }
}
