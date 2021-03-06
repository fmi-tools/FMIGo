#include "sc/Connector.h"
#include "sc/LockConstraint.h"
#include "sc/BallJointConstraint.h"
#include "sc/Equation.h"
#include "stdio.h"
#include "stdlib.h"

using namespace sc;

LockConstraint::~LockConstraint(){}

LockConstraint::LockConstraint(
    Connector* connA,
    Connector* connB,
    const Vec3& localAnchorA,
    const Vec3& localAnchorB,
    const Quat& localOrientationA,
    const Quat& localOrientationB
) : BallJointConstraint(connA,connB,localAnchorA,localAnchorB),
    m_xr(connA, connB),
    m_yr(connA, connB),
    m_zr(connA, connB)
{
    addEquation(&m_xr);
    addEquation(&m_yr);
    addEquation(&m_zr);

    // 3 equations, one for each rotational DOF
    m_xr.setG( 0, 0, 0,-1, 0, 0, 0, 0, 0, 1, 0, 0);
    m_yr.setG( 0, 0, 0, 0,-1, 0, 0, 0, 0, 0, 1, 0);
    m_zr.setG( 0, 0, 0, 0, 0,-1, 0, 0, 0, 0, 0, 1);
}

void LockConstraint::update(){
    // Rotational violation:
    //      g = ni .dot( nj )  (3 rotational DOFs)
    // where
    //      ni is a unit vector that rotates with body i
    //      nj is a unit vector that rotates with body j

    Vec3 x(1,0,0);
    Vec3 y(0,1,0);
    Vec3 z(0,0,1);
    Vec3 zero(0,0,0);

    Vec3 xi = m_connectors[0]->m_quaternion . multiplyVector( x );
    Vec3 yi = m_connectors[0]->m_quaternion . multiplyVector( y );
    Vec3 zi = m_connectors[0]->m_quaternion . multiplyVector( z );

    Vec3 xj = m_connectors[1]->m_quaternion . multiplyVector( x );
    Vec3 yj = m_connectors[1]->m_quaternion . multiplyVector( y );
    Vec3 zj = m_connectors[1]->m_quaternion . multiplyVector( z );

    int i = -1;

    Vec3 zj_x_yi = zj.cross(yi);
    m_xr.setG(zero, zj_x_yi*i, zero, zj_x_yi*(-i));
    m_xr.setViolation(yi.dot(zj));

    Vec3 xj_x_zi = xj.cross(zi);
    m_yr.setG(zero, xj_x_zi*i, zero, xj_x_zi*(-i));
    m_yr.setViolation(zi.dot(xj));

    Vec3 yj_x_xi = yj.cross(xi);
    m_zr.setG(zero, yj_x_xi*i, zero, yj_x_xi*(-i));
    m_zr.setViolation(xi.dot(yj));

    BallJointConstraint::update();
}
